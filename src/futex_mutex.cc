#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <immintrin.h>
#include <errno.h>
#include <thread>
#include <time.h>

#include "futex_mutex.h"

static const int lock_spin_count = 100;
static const int unlock_spin_count = 200;

// Return 0 if it's a fast acquiistion, 1 if slow
extern "C" int futex_mutex_lock(futex_mutex_t *m) {
  pthread_mutex_lock(&m->mutex);
  m->n_locking++;
  while (m->locked) {
    pthread_cond_wait(&m->locking, &m->mutex);
  }
  m->n_locking--;
  m->locked = true;
  pthread_mutex_unlock(&m->mutex);
  return 0;
}
 
extern "C" void futex_mutex_unlock(futex_mutex_t *m) {
  pthread_mutex_lock(&m->mutex);
  m->locked = 0;
  if (m->n_locking) {
    pthread_cond_signal(&m->locking);
  } else if (m->n_waiting) {
    pthread_cond_broadcast(&m->waiting);
  }
  pthread_mutex_unlock(&m->mutex);
}

extern "C" int futex_mutex_subscribe(futex_mutex_t *m) {
  return m->locked;
}

extern "C" int futex_mutex_wait(futex_mutex_t *m) {
  for (int i = 0; i < lock_spin_count; i++) {
    if (!m->locked) return 0; //  We can be a little opportunistic here.
    _mm_pause();
  }
  // Now we have to do the relatively heavyweight thing.
  pthread_mutex_lock(&m->mutex);
  m->n_waiting++;
  while (m->locked) {
    pthread_cond_wait(&m->waiting, &m->mutex);
  }
  m->n_waiting--;
  pthread_mutex_unlock(&m->mutex);
  return 1;
}
  
#ifdef TESTING
futex_mutex_t m;
static void foo() {
  futex_mutex_lock(&m);
  printf("foo sleep\n");
  sleep(2);
  printf("foo slept\n");
  futex_mutex_unlock(&m);
}

static void simple_test() {
  std::thread a(foo);
  std::thread b(foo);
  std::thread c(foo);
  a.join();
  b.join();
  c.join();
}

static bool time_less(const struct timespec &a, const struct timespec &b) {
  if (a.tv_sec < b.tv_sec) return true;
  if (a.tv_sec > b.tv_sec) return false;
  return a.tv_nsec < b.tv_nsec;
}

volatile int exclusive_is_locked=0;
volatile uint64_t exclusive_count=0;

static void stress() {
  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  start.tv_sec ++;
  uint64_t locked_fast=0, locked_slow=0, sub_locked=0, sub_unlocked=0, wait_long=0, wait_short=0, wait_was_one=0, wait_was_zero=0;
  while (1) {
    clock_gettime(CLOCK_MONOTONIC, &end);
    if (time_less(start, end)) break;
    for (uint64_t i = 0; i < 100; i++) {
      switch (i%3) {
	case 0: {
	  int lock_kind = futex_mutex_lock(&m);
	  if (0) {
	    assert(!exclusive_is_locked);
	    exclusive_is_locked=1;
	    exclusive_count++;
	    assert(exclusive_is_locked);	  
	    exclusive_is_locked=0;
	  }
	  futex_mutex_unlock(&m);
	  if (lock_kind==0) locked_fast++;
	  else              locked_slow++;
	  break;
	}
	case 1:
	  if (futex_mutex_subscribe(&m)) {
	    sub_locked++;
	  } else {
	    sub_unlocked++;
	  }
	  break;
	case 2:
	  if  (futex_mutex_wait(&m)) {
	    wait_long++;
	  } else {
	    wait_short++;
	  }
	  if (m.locked) {
	    wait_was_one++;
	  } else {
	    wait_was_zero++;
	  }
	  break;
      }
    }
  }
  printf("locked_fast=%8ld locked_slow=%8ld sub_locked=%8ld sub_unlocked=%8ld wait_long=%8ld wait_short=%8ld was1=%8ld was0=%ld\n", locked_fast, locked_slow, sub_locked, sub_unlocked, wait_long, wait_short, wait_was_one, wait_was_zero);
}

static void stress_test() {
  const int n = 8;
  std::thread x[n];
  for (int i = 0; i < n; i++) { 
    x[i] = std::thread(stress);
  }
  for (int i = 0; i < n; i++) {
    x[i].join();
  }
}
  

extern "C" void test_futex() {
  stress_test();
  simple_test();
}
#endif

