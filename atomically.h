#ifndef ATOMICALLY_H
#define ATOMICALLY_H

#include <sched.h>
#include <stdbool.h>
#include <immintrin.h>

#include "rng.h"


#ifdef __cplusplus
extern "C" {
#endif

static inline bool mylock_wait(volatile unsigned int *mylock) {
  bool too_long = false;
  while (*mylock) {
    if (0==(prandnum()&(1024-1))) {
      sched_yield();
      too_long = true;
    } else {
      __asm__ volatile("pause");
    }
  }
  return too_long;
}

static inline unsigned int xchg(volatile unsigned int *addr, unsigned int newval) {
  unsigned int result;
  __asm__ volatile("lock xchgl %0, %1" :
		   "+m"(*addr), "=a" (result) :
		   "1" (newval) :
		   "cc");
  return result;
}

static inline void mylock_acquire(volatile unsigned int *mylock) {
  do {
    mylock_wait(mylock);
  } while (xchg(mylock, 1));
}

static inline void mylock_release(volatile unsigned int *mylock) {
  *mylock = 0;
}

#define XABORT_LOCK_HELD 9

#define have_rtm 0

static inline void atomically(volatile unsigned int *mylock,
			      void (*predo)(void *extra),
			      void (*fun)(void*extra),
			      void*extra) {
  int count = 0;
  while (have_rtm && count < 20) {
    mylock_wait(mylock);
    predo(extra);
    while (mylock_wait(mylock)) {
      // If the lock was held for a long time, then do the predo code again.
      predo(extra);
    }
    unsigned int xr = _xbegin();
    if (xr == _XBEGIN_STARTED) {
      fun(extra);
      if (*mylock) _xabort(XABORT_LOCK_HELD);
      _xend();
      return;
    } else if ((xr & _XABORT_EXPLICIT) && (_XABORT_CODE(xr) == XABORT_LOCK_HELD)) {
      count = 0; // reset the counter if we had an explicit lock contention abort.
      continue;
    } else {
      count++;
      for (int i = 1; i < count; i++) {
	if (0 == (prandnum()&1023)) {
	  sched_yield();
	} else {
	  __asm__ volatile("pause");
	}
      }
    }
  }
  // We finally give up and acquire the lock.
  mylock_acquire(mylock);
  fun(extra);
  mylock_release(mylock);
}

#define atomic_load(addr) __atomic_load_n(addr, __ATOMIC_CONSUME)

#define prefetch_read(addr) __builtin_prefetch(addr, 0, 3)
#define prefetch_write(addr) __builtin_prefetch(addr, 1, 3)

#ifdef __cplusplus
}
#endif
#endif // ATOMICALLY_H
