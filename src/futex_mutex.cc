#include <assert.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <limits.h>
#include <unistd.h>
#include <immintrin.h>
#include <errno.h>
#include <thread>
#include <time.h>

#include "futex_mutex.h"

// the lock field:  bit 1 is the lock
//                  bit 2 says someone is "waiting"
//                  the rest of the bits are the count for the number waiting to lock.
const int increment_locker_by = 4;

static long sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
  return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

static long futex_wait(volatile int *addr, int val) {
  return sys_futex((void*)addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0);
}
static long futex_wake1(volatile int *addr) {
  return sys_futex((void*)addr, FUTEX_WAKE_PRIVATE, 1,   NULL, NULL, 0);
}
static long futex_wakeN(volatile int *addr) {
  return sys_futex((void*)addr, FUTEX_WAKE_PRIVATE, INT_MAX,   NULL, NULL, 0);
}

static const int lock_spin_count = 100;
static const int unlock_spin_count = 200;

// Return 0 if it's a fast acquiistion, 1 if slow
extern "C" int futex_mutex_lock(futex_mutex_t *m) {
  int count = 0;
  while (count < lock_spin_count) {
    int old_c = m->lock;
    if ((old_c & 1) == 1) {
      // Someone else has the lock, so we are spinning.
      _mm_pause();
      count++;
    } else if (__sync_bool_compare_and_swap(&m->lock, old_c, old_c | 1)) {
      // No one else had the lock, and we successfully grabbed it.
      return 0;
    } else {
      // Someone else modified old_c while we were running.  So we just want to try again, without incrementing count or pausing
      continue;
    }
  }
  
  // We got here without getting the lock, so let's add ourselves to the count.
  __sync_fetch_and_add(&m->lock, increment_locker_by);

  // Now we must wait for the lock to go free.  We'll use the futex, but we'll be opportunistic if value changed.
  bool did_futex = false;
  while (1) {
    int old_c = m->lock;
    if ((old_c & 1) == 1) {
      // Someone else has the lock
      futex_wait(&m->lock, old_c); // we don't care if the futex fails because old_c changed, we'll just go again anyway.
      did_futex = true;
    } else if (__sync_bool_compare_and_swap(&m->lock, old_c, old_c-increment_locker_by +1)) {
      // No one else had the lock, and we managed to grab it (decrementing by 3 has the effect of subtracting 4 (to indicate that we are no longer waiting) and setting the lock bit.
      return did_futex;
    } else {
      // No one else had the lock, but someone modified it while we were trying to lock, so just try again without pausing
      continue;
    }
  }
}   
  
extern "C" void futex_mutex_unlock(futex_mutex_t *m) {
  while (1) {
    int old_m = m->lock;
    if (__sync_bool_compare_and_swap(&m->lock, old_m, old_m & ~1)) {
      if ((old_m & ~3) == 0) {
	// No one was waiting to lock, so it's unlocked with no contention.  
	if (m->wait) {
	  // Wake up anyone who is waiting for 0.
	  m->wait = 0;
	  __sync_fetch_and_and(&m->lock, ~2); // no one is waiting on the lock now.
	  futex_wakeN(&m->wait);
	}
	return;
      } else {
	break;             // someone may be waiting, so we'll have to wake them up.
      }
    }
  }
  // Spin a little, hoping that someone takes the lock.
  for (int i = 0; i < unlock_spin_count; i++) {
    if (m->lock & 1) return; // someone else took the lock, so it will wake anyone that needs waking when it's done.
    _mm_pause();
  }
  // No one took it, so we have to wake someone up.
  futex_wake1(&m->lock);
}

extern "C" int futex_mutex_subscribe(futex_mutex_t *m) {
  return (m->lock)&1;
}

extern "C" int futex_mutex_wait(futex_mutex_t *m) {
  while (1) {
    for (int count = 0; count < lock_spin_count; count++) {
      if ((m->lock & 1) == 0) return false; // it was quick
      _mm_pause();
    }
    // So we resign ourselves to waiting
    
    if ((m->lock & 2) == 0) {
      __sync_fetch_and_or(&m->lock, 2); // tell the lock I'm waiting
    }
    m->wait = 1;
    futex_wait(&m->wait, 1);
  }
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
	  if (m. lock & 1) {
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

