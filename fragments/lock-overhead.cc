/* Measure atomicity overhead for three cases:
 *   We want to push and pop things from a list.
 *  Case 1: A global list.
 *  Case 2: A per-cpu list.
 *  Case 3: A per-thread list.

On x240 (i7-4600U 2.10GHz)
$ ./lock-overhead 
 global_list: 13.810500s
     per_cpu:  1.698616s
  per_thread:  0.331605s
    in_stack:  0.302764s
 */
#include <cassert>
#include <ctime>
#include <thread>
#include <immintrin.h>

static double tdiff(const struct timespec &a, const struct timespec &b) {
  return (b.tv_sec - a.tv_sec) + 1e-9*(b.tv_nsec - a.tv_nsec);
}

static void timeit_fun(void(*f)(), const char *name) {
  struct timespec start,end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  std::thread a(f);
  std::thread b(f);
  a.join();
  b.join();
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("%12s: %9.6fs\n", name, tdiff(start, end));
}

#define timeit(name) timeit_fun(name, #name)

struct datastruct {
  volatile int   value __attribute__((aligned(64)));
  volatile int   lock;
};

static void atomic_do(datastruct *l) {
  while (__sync_lock_test_and_set(&l->lock, 1)) {
    _mm_pause();
  }
  l->value++;
  __sync_lock_release(&l->lock);
}
   
static datastruct  the_global_list; 

const int N_iterations = 100000000;

static void global_list() {
  for (int i = 0; i < N_iterations; i++) {
    atomic_do(&the_global_list);
  }
}



static datastruct cpu_lists[128];

struct getcpu_cache {
  uint32_t cpu, count;
};
static uint32_t getcpu(getcpu_cache *c) {
  if ((c->count++)%64  ==0) { c->cpu = sched_getcpu(); }
  return c->cpu;
}


static void per_cpu() {
  getcpu_cache c = {0,0};
  for (int i = 0; i < N_iterations; i++) {
    atomic_do(&cpu_lists[getcpu(&c)]);
  }
}

static __thread datastruct thread_list;
static void per_thread() {
  for (int i = 0; i < N_iterations; i++) {
    thread_list.value++;
  }
}

static void in_stack() {
  datastruct stack_list = {0, 0};
  for (int i = 0; i < N_iterations; i++) {
    stack_list.value++;
  }
}

int main(int argc, char *argv[] __attribute__((unused))) {
  assert(argc == 1);
  timeit(global_list);
  timeit(per_cpu);
  timeit(per_thread);
  timeit(in_stack);
  return 0;
}
