/* Simulate a server load.
 * A fixed number of threads.
 * Each thread owns a set of objects.
 * Each thread repeatedly:
 *   frees one of its objects
 *   allocates and initializes another object
 *   gives the object to one of the other threads.
 * The objects are chosen with a exponential size distribution (size 1 objects are twice as likely as size 2 objects)
 * The odds that an object is freed is also inversely proportional to its size.
 * We accomplish this by setting the lifetime of an object to be a random number chosen from 1 to O(size).
 */

#include "random.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <queue>
#include <sys/resource.h>
#include <thread>
#include <unistd.h>
#include <valgrind/helgrind.h>

struct object {
    int64_t expiration_date;
    size_t  size;
    void   *data;
    int64_t time_to_alloc;
    bool operator<(const object &other) const {
        return expiration_date > other.expiration_date;
    }
    object(int64_t e, size_t s) {
        expiration_date = e;
        size            = s;
        struct timespec start,end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        data            = malloc(s);
	memset(data, 3, s);
        clock_gettime(CLOCK_MONOTONIC, &end);
        time_to_alloc = (end.tv_sec - start.tv_sec) * 1000000000ul + (end.tv_nsec - start.tv_nsec); 
    }
    ~object() {
        if (data) {
	  free(data);
        }
    }
};

class thread_info {
  public:
    object *incoming_object;
    std::priority_queue<object*> objects;
    int64_t a_count; // how many objects were allocated?
    int64_t d_count; // how many deallocated?
    lran2_st lran;
    thread_info() {
        incoming_object = 0;
    }
    ~thread_info() {
        if (incoming_object) delete incoming_object;
        while (!objects.empty()) {
            object *o = objects.top();
            objects.pop();
            delete o;
        }
    }
};

long n_mallocs = 10;
int n_threads = 4;
int64_t memory_total = 1u<<28;
int64_t memory_per_thread;
thread_info *thread_infos;
std::thread *threads;

static size_t pick_size_and_life(thread_info *ti, int64_t *life)
// Effect: Return a random number which is exponentially distributed.
//   Specifically, pick a number for which ceiling(lg(n)) is uniformly distributed.
//   Within each power of two, make the sizes uniformly distributed.
{
    unsigned long r = lran2(&ti->lran);
    int l;
    for (l=1; l<23; l++) {
        if (r%2==0) break;
        r = r/2;
    }
    if (l<4) l=4; // avoid the very small sizes, on which supermalloc may be gaining an unfair advantage by returning misaligned objects.
    unsigned long result = (1<<l) + (lran2(&ti->lran)&((1<<l)-1));
    *life = result + (lran2(&ti->lran)&((1<<l)-1));
    return result;
}
    
// This should go on its own cache line
struct stop_flag {
    bool stop_flag;
} __attribute__((aligned(32))) stop_flag;

static bool get_stop_flag(void) {
    // don't need valgrind annotations on the read, since the write has them.
    // But bad things happen without them.
    VALGRIND_HG_DISABLE_CHECKING(&stop_flag, sizeof(stop_flag));
    bool o =  __atomic_load_n(&stop_flag.stop_flag, __ATOMIC_CONSUME);
    VALGRIND_HG_ENABLE_CHECKING(&stop_flag, sizeof(stop_flag));
    return o;
}
static void set_stop_flag(bool v) {
    VALGRIND_HG_DISABLE_CHECKING(&stop_flag, sizeof(stop_flag));
    __atomic_store_n(&stop_flag.stop_flag, v, __ATOMIC_RELEASE);
    VALGRIND_HG_ENABLE_CHECKING(&stop_flag, sizeof(stop_flag));
}


static long maxrss = -1;

static long getrss(void) {
    struct rusage ru;
    int r = getrusage(RUSAGE_SELF, &ru);
    assert(r==0);
    long this_max = ru.ru_maxrss;;
    while (1) {
        long old_max = maxrss;
        if (old_max >= this_max) break;
        if (__sync_bool_compare_and_swap(&maxrss, old_max, this_max)) break;
    }
    return this_max;
}

static int64_t a_count_limit = 10000000;

static void run_malloc_run(thread_info *ti) {
    int64_t a_count = 0;
    int64_t d_count = 0;
    int64_t total_size_for_this_thread = 0;
    int64_t biggest_a_time = -1, biggest_d_time = -1;
    while (!get_stop_flag()) {
        if (a_count > a_count_limit) set_stop_flag(true);
        if (a_count % (1024*16) == 0) { printf("."); fflush(stdout); getrss(); }
	int64_t life;
        size_t  s = pick_size_and_life(ti, &life);
        int64_t e = a_count+life;
        object *o = new object(e,s);
        assert(o->time_to_alloc >= 0);
        if (o->time_to_alloc > biggest_a_time) {
            biggest_a_time = o->time_to_alloc;
            long maxrss = getrss();
            if (0) printf("biggest time = %ld (rss=%ld)\n", biggest_a_time, maxrss);
        }
        a_count++;
        int    other_number = random()%n_threads;
        // Give o onto the other thread.  Meanwhile, if we receive something add it to our heap.
        thread_info *oti = &thread_infos[other_number];
        // Try to store
        while (1) {
            VALGRIND_HG_DISABLE_CHECKING(&oti->incoming_object, sizeof(oti->incoming_object));
            bool pushed_it =  __sync_bool_compare_and_swap(&oti->incoming_object, NULL, o);
            VALGRIND_HG_ENABLE_CHECKING(&oti->incoming_object, sizeof(oti->incoming_object));
            if (pushed_it) break;
            
            //while (!)__sync_bool_compare_and_swap(&oti->incoming_object, NULL, o) {

            if (get_stop_flag()) {
                delete o;
                break;
            }

            // The store failed because the other guy has something.
            // To avoid deadlock, we've got to remove our incoming object, then go try again.
            VALGRIND_HG_DISABLE_CHECKING(&ti->incoming_object, sizeof(ti->incoming_object));
            object *my_o = __atomic_load_n(&ti->incoming_object, __ATOMIC_CONSUME);
            VALGRIND_HG_ENABLE_CHECKING(&ti->incoming_object, sizeof(ti->incoming_object));
            if (my_o) {
                VALGRIND_HG_DISABLE_CHECKING(&ti->incoming_object, sizeof(ti->incoming_object));
                __atomic_store_n(&ti->incoming_object, NULL, __ATOMIC_RELEASE);
                VALGRIND_HG_ENABLE_CHECKING(&ti->incoming_object, sizeof(ti->incoming_object));
                ti->objects.push(my_o);
                total_size_for_this_thread += my_o->size;
            } else {
                __asm__ volatile ("pause");
            }
        }
        while (total_size_for_this_thread > memory_per_thread) {
            assert(!ti->objects.empty());
            object *kill_me = ti->objects.top();
            ti->objects.pop();
            total_size_for_this_thread -= kill_me->size;
            assert(total_size_for_this_thread > 0);
            struct timespec start,end;
            clock_gettime(CLOCK_MONOTONIC, &start);
            kill_me->data = NULL;
            delete kill_me;
            clock_gettime(CLOCK_MONOTONIC, &end);
            int64_t time_to_free = (end.tv_sec - start.tv_sec) * 1000000000ul + (end.tv_nsec - start.tv_nsec);
            assert(time_to_free >= 0);
            if (time_to_free > biggest_d_time) {
                biggest_d_time = time_to_free;
                printf("big d time = %ld\n", biggest_d_time);
            }
            d_count++;
        }
    }
    getrss();
    ti->a_count = a_count;
    ti->d_count = d_count;
    printf("Total_size_for_this_thread=%ld big_a=%ld big_d=%ld\n", total_size_for_this_thread,
	   biggest_a_time, biggest_d_time);
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    printf("currentrss=%ld\n", getrss());
    memory_per_thread = memory_total/n_threads;
    thread_infos = new thread_info[n_threads];
    threads      = new std::thread[n_threads];

    set_stop_flag(false);
    for (int i = 0; i < n_threads; i++) {
        lran2_init(&thread_infos[i].lran, i+1);
        threads[i] = std::thread(run_malloc_run, &thread_infos[i]);
    }

    if (1) {
        sleep(10);
	printf("before stopping: currentrss=%ldMiB\n", getrss()/1024);
        set_stop_flag(true);
    }

    for (int i = 0; i < n_threads; i++) {
        threads[i].join();
        printf("a=%ld d=%ld, ", thread_infos[i].a_count, thread_infos[i].d_count);
    }
    printf("\n");

    for (int i = 0; i < n_threads; i++) {
        int64_t sum_this_thread = 0;
        while (!thread_infos[i].objects.empty()) {
            object *o = thread_infos[i].objects.top();
            sum_this_thread += o->size;
            thread_infos[i].objects.pop();
            delete o;
        }
        printf ("thread sum =%ld\n", sum_this_thread);
    }
    printf("before deleting: currentrss=%ldMiB\n", getrss()/1024);
    delete [] threads;
    delete [] thread_infos;
    printf("currentrss=%ldMiB\n", getrss()/1024);
    printf("maxrss=%ldMiB\n", maxrss/1024);
    return 0;
}
