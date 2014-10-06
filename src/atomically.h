#ifndef ATOMICALLY_H
#define ATOMICALLY_H

#include <sched.h>
#include <stdbool.h>
#include <immintrin.h>

#include "rng.h"


static inline bool mylock_wait(volatile unsigned int *mylock) {
  bool too_long = false;
  while (*mylock) {
    if (0==(prandnum()&(1024-1))) {
      sched_yield();
      too_long = true;
    } else {
        _mm_pause();
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

class mylock_raii {
  volatile unsigned int *mylock;
public:
  mylock_raii(volatile unsigned int *mylock) : mylock(mylock) {
    mylock_acquire(mylock);
  }
  ~mylock_raii() {
    mylock_release(mylock);
  }
};

extern bool use_transactions;
extern bool do_predo;

#define XABORT_LOCK_HELD 9

#ifdef COVERAGE
#define have_rtm 0
#else
#define have_rtm use_transactions
#endif

struct atomic_stats_s {
  uint64_t atomic_count __attribute__((aligned(64)));
  uint64_t locked_count;
};
extern struct atomic_stats_s atomic_stats;

template<typename ReturnType, typename... Arguments>
static inline ReturnType atomically(volatile unsigned int *mylock,
			            void (*predo)(Arguments... args),
				    ReturnType (*fun)(Arguments... args),
				    Arguments... args) {
  __sync_fetch_and_add(&atomic_stats.atomic_count, 1);
  if (have_rtm) {
    // Be a little optimistic: try to run the function without the predo if we the lock looks good
    if (*mylock == 0) {
      unsigned int xr = _xbegin();
      if (xr == _XBEGIN_STARTED) {
	if (*mylock) _xabort(XABORT_LOCK_HELD);
	ReturnType r = fun(args...);
	_xend();
	return r;
      }
    }

    int count = 0;
    while (count < 10) {
      mylock_wait(mylock);
      if (do_predo) predo(args...);
      while (mylock_wait(mylock)) {
	// If the lock was held for a long time, then do the predo code again.
	if (do_predo) predo(args...);
      }
      unsigned int xr = _xbegin();
      if (xr == _XBEGIN_STARTED) {
	ReturnType r = fun(args...);
	if (*mylock) _xabort(XABORT_LOCK_HELD);
	_xend();
	return r;
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
  }
  // We finally give up and acquire the lock.
  __sync_fetch_and_add(&atomic_stats.locked_count, 1);
  if (do_predo) predo(args...);
  mylock_raii mr(mylock);
  ReturnType r = fun(args...);
  return r;
}

struct lock {
  unsigned int l __attribute((aligned(64)));
};

#define atomic_load(addr) __atomic_load_n(addr, __ATOMIC_CONSUME)
#define atomic_store(addr, val) __atomic_store_n(addr, val, __ATOMIC_RELEASE)

#define prefetch_read(addr) __builtin_prefetch(addr, 0, 3)
#define prefetch_write(addr) __builtin_prefetch(addr, 1, 3)

static inline void fetch_and_max(uint64_t * ptr, uint64_t val) {
  while (1) {
    uint64_t old = atomic_load(ptr);
    if (val <= old) return;
    if (__sync_bool_compare_and_swap(ptr, old, val)) return;
  }
}

#endif // ATOMICALLY_H
