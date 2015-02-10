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

 ***********************************************
 Copyright 2014 Bradley C. Kuszmaul.

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * so-called Expat license.

 */
#include <cassert>
#include <cstdio>
#include <ctime>
#include <thread>
#include <immintrin.h>

static double tdiff(const struct timespec &a, const struct timespec &b) {
  return (b.tv_sec - a.tv_sec) + 1e-9*(b.tv_nsec - a.tv_nsec);
}

const int N_iterations = 100000000;

static void timeit(void(*f)(), const char *name) {
  struct timespec start,end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  std::thread a(f);
  std::thread b(f);
  a.join();
  b.join();
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("%55s & %8.1fns \\\\\n", name, (tdiff(start, end)/N_iterations) * 1e9);
}

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

static void global_list() {
  for (int i = 0; i < N_iterations; i++) {
    atomic_do(&the_global_list);
  }
}



static datastruct cpu_lists[128];

struct getcpu_cache {
  uint32_t cpu, count;
};
static const int cpu_cache_recheck_freq = 32;
static uint32_t getcpu(getcpu_cache *c) {
  if ((c->count++)%cpu_cache_recheck_freq  ==0) { c->cpu = sched_getcpu(); }
  return c->cpu;
}


static void per_cachedcpu() {
  getcpu_cache c = {0,0};
  for (int i = 0; i < N_iterations; i++) {
    atomic_do(&cpu_lists[getcpu(&c)]);
  }
}

static void per_schedcpu() {
  for (int i = 0; i < N_iterations; i++) {
    atomic_do(&cpu_lists[sched_getcpu()]);
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
  timeit(global_list,    "global list");
  timeit(per_schedcpu,   "per cpu (\\mintinline{c}{sched\\_cpu()} every time)");
  char name[99];
  snprintf(name, sizeof(name), "per cpu (cache getcpu, refresh every %d", cpu_cache_recheck_freq);
  timeit(per_cachedcpu,  name); 
  timeit(per_thread,     "per thread");
  timeit(in_stack,       "local in stack");
  return 0;
}
