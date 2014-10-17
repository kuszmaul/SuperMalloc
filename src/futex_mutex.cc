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

#define atomic_load(addr) __atomic_load_n(addr, __ATOMIC_CONSUME)
#define atomic_store(addr, val) __atomic_store_n(addr, val, __ATOMIC_RELEASE)

// The lock field is the 2*number who currently wait to lock the lock + 1 if the lock is locked.
// The hold field is the number waiting for the lock to zero (with the intention of running a transaction)

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

static const int lock_spin_count = 20;
static const int unlock_spin_count = 20;

// Return 0 if it's a fast acquiistion, 1 if slow
extern "C" int futex_mutex_lock(futex_mutex_t *m) {
  int count = 0;
  while (count < lock_spin_count) {
    int old_c = atomic_load(&m->lock);
    if ((old_c & 1) == 1) {
      // someone else has the lock, so spin
      _mm_pause();
      count++;
    } else if (__sync_bool_compare_and_swap(&m->lock, old_c, old_c+1)) {
      // got it
      return 0;
    } else {
      continue;
    }
  }
  // We got here without getting the lock, so we must do the futex thing.
  __sync_fetch_and_add(&m->lock, 2); // increase the count.
  int did_futex = 0;
  while (1) {
    int old_c = atomic_load(&m->lock);
    if ((old_c & 1) == 1) {
      futex_wait(&m->lock, old_c);
      did_futex = 1;
    } else if (__sync_bool_compare_and_swap(&m->lock, old_c, old_c-1)) {
	// We got the lock (decrementing by 1 is the same as decrementing by 2 and setting the lock bit.
	return did_futex;
    } else {
      // go try again.
      continue;
    }
  }
}
 
int threadcount=0;
__thread int last_futex_wait_result = 0, last_m_wait, last_errno, threadid=-1;

static void set_threadid() {
  if (threadid==-1) threadid = __sync_fetch_and_add(&threadcount, 1);
}

extern "C" void futex_mutex_unlock(futex_mutex_t *m) {
  set_threadid();
  int old_c = __sync_fetch_and_add(&m->lock, -1);
  if (old_c > 1) {
    // There were multiple waiting
    //
    // Some implementations wait around to see if someone else will
    // grab the lock.  We're not doing that, we're going straight to
    // the wakeup if needed.
    futex_wake1(&m->lock);
  } else {
    while (1) {
      int old_hold = atomic_load(&m->hold);
      // If old_c == n1, then maybe someone is waiting to run a transaction.  Wake up all the waiters.
      if (old_hold) {
	futex_wakeN(&m->hold);
      } else {
	break;
      }
    }
  }
}

extern "C" int futex_mutex_subscribe(futex_mutex_t *m) {
  return atomic_load(&m->lock) & 1;
}

extern "C" int futex_mutex_hold(futex_mutex_t *m) {
  //set_threadid();
  for (int i = 0; i < lock_spin_count; i++) {
    if (atomic_load(&m->lock) == 0) return false;
    _mm_pause();
  }
  int did_futex = 0;

  // Now we have to do the relatively heavyweight thing.
  __sync_fetch_and_add(&m->hold, 1);
  while (1) {
    // Make this be an atomic fetch, just to make sure.
    int was_locked = atomic_load(&m->lock);
    if (was_locked == 0) {
      __sync_fetch_and_add(&m->hold, -1);
      return did_futex;
    }
    // usleep(1000); // putting in this sleep makes it fail a lot.
    last_futex_wait_result = futex_wait(&m->hold, 1);
    last_errno = errno;
    did_futex = 1;
  }
}
  
// Can I argue that the futex_wait  never hangs?  
//  The race that can happen is that 
//   the wait checks that the lock is held
//   then the unlock sets the lock to 0, sets wait to 0, and then issues a wakeN (to the empty set)
//   then the wait sets wait to 1 and hangs forever.

// What if wait first sets wait (as in the code above), checks the lock and then waits on the futex.  Leaving the extra wait doesn't really cost anything if the lock happens to clear.
//
//      waiter        unlocker
//      wait:=1
//      1==lock
//                   lock:=0
//                   check wait (seeing the wait)
//                   wait:=0
//                   wake(wait)
//      -1==wait(wait)                                the futex fails since wait has been changed
//      wait:=1
//      0==lock                                       done!
//
// Another interleaving
//      waiter        unlocker
//      wait:=1
//      1==lock
//      wait
//                   lock:=0
//                   check wait (seeing the wait)
//                   wait:=0
//                   wake(wait)
//      returns                                       done!
//
// I've observed lock=0, wait=1 and the futex_mutex_wait stalled.  How could that have happened?
// Even with everything made atomic and adding a fence between the store to wait and the load from lock in mutex_wait
// Another interleaving
//       waiter             unlocker
//
//       futex_wait(w==1)
//                          l:=0
//                          observe w==1
//                          set w:=0 (that's not what we saw)
//
// So it must be that the l:=0 happens before
//       waiter             unlocker
//
//       w:=1
//       observe(l!=0)
//                          l:=0
//                          observe w==1
//                          w:=0
//                          wake(w)
//       
//       futex_wait(w==1)                  fails since w==0
//
//
// Finally found the bug:  Takes three threads.  State = (lock, hold)
//   hold                lock/unlock              hold
//   -----------------------------------------------------------
// A:                      lock: state=(1,0)
// B:  hold:=1 (1,1)                                                              depends on A
// C:                                             lock==1[continue after spin]    depends on A
// D:  lock==0[continue] 
// E:                      unlock: (0,1)                                          depends on D
// F:                      hold:=0 (0,0)
// G:                      wake()
// H:                                             hold:=1                         depends on F
// I:                                             lock==0[return]
// J: wait(stalls)                                                                depends on G and H

// Some instrumentation shows that we have just had a failure from futex_wait
// with EWOULDBLOCK (11 which is also EAGAIN)
//   So it goes around and sets m->hold to 1 again
//   but now m->lock is 1 so it does another futex wait.



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
	  if  (futex_mutex_hold(&m)) {
	    wait_long++;
	  } else {
	    wait_short++;
	  }
	  if (futex_mutex_subscribe(&m)) {
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

