/* Measure atomicity overhead for three cases:
 *   We want to push and pop things from a list.
 *  Case 1: A global list.
 *  Case 2: A per-cpu list.
 *  Case 3: A per-thread list.
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

struct pair {
  pair *next;
};

struct locked_list {
  pair *list __attribute__((aligned(64)));
  int   lock;
};

static void nonatomic_push(locked_list *l, pair *n) {
  n->next = l->list;
  l->list = n;
}
static void atomic_push(locked_list *l, pair *n) {
  while (__sync_lock_test_and_set(&l->lock, 1)) {
    _mm_pause();
  }
  nonatomic_push(l, n);
  __sync_lock_release(&l->lock);
}
static pair* nonatomic_pop(locked_list *l) {
  pair *result = l->list;
  if (result) {
    l->list = result->next;
  }
  return result;
}

static pair* atomic_pop(locked_list *l) {
  while (__sync_lock_test_and_set(&l->lock, 1)) {
    _mm_pause();
  }
  pair *result = nonatomic_pop(l);
  __sync_lock_release(&l->lock);
  return result;
}

   
static locked_list  the_global_list; 

const int N_iterations = 1000000;

static void global_list() {
  for (int i = 0; i < 100; i++) {
    atomic_push(&the_global_list, new pair);
  }
  for (int i = 0; i < N_iterations; i++) {
    pair *l = atomic_pop(&the_global_list);
    if (l) {
      atomic_push(&the_global_list, l);
    }
  }
}

static locked_list cpu_lists[128];
static void per_thread() {
  for (int i = 0; i < 100; i++) {
    nonatomic_push(&thread_list, new pair);
  }
  for (int i = 0; i < N_iterations; i++) {
    pair *l = nonatomic_pop(&thread_list);
    if (l) {
      nonatomic_push(&thread_list, l);
    }
  }
}


static __thread locked_list thread_list;

static void per_thread() {
  for (int i = 0; i < 100; i++) {
    nonatomic_push(&thread_list, new pair);
  }
  for (int i = 0; i < N_iterations; i++) {
    pair *l = nonatomic_pop(&thread_list);
    if (l) {
      nonatomic_push(&thread_list, l);
    }
  }
}

static void in_stack() {
  locked_list stack_list = {NULL, 0};
  for (int i = 0; i < 100; i++) {
    nonatomic_push(&stack_list, new pair);
  }
  for (int i = 0; i < N_iterations; i++) {
    pair *l = nonatomic_pop(&stack_list);
    if (l) {
      nonatomic_push(&stack_list, l);
    }
  }
}

int main(int argc, char *argv[] __attribute__((unused))) {
  assert(argc == 1);
  timeit(global_list);
  while (1) {
    pair *l = nonatomic_pop(&the_global_list);
    if (l == NULL) break;
    delete l;
  }
  //timeit(per_cpu);
  timeit(per_thread);
  timeit(in_stack);
  return 0;
}
