#ifndef ATOMICALLY_H
#define ATOMICALLY_H

#include <sched.h>
#include <stdbool.h>
#include <immintrin.h>

#include "bassert.h"
#include "rng.h"

#include <pthread.h>
#include "futex_mutex.h"

union lock_t {
  pthread_mutex_t pt_m __attribute__((aligned(64)));
  futex_mutex_t f_m __attribute((aligned(64)));
};

enum mutex_mode_t { MODE_PTHREAD_MUTEX, MODE_TSX };

extern mutex_mode_t mode;

// Pthread mutexes are larger than Futex mutexes, and the first two fields
// get initialized to zero, covering what the Futex needs to be initialized
// as.  This is highly implementation dependent, but it's hard to change
// the pthread_mutex_t implementation with so much legacy code, so we're
// probably pretty safe doing this.
#define LOCK_INITIALIZER PTHREAD_MUTEX_INITIALIZER // Pthread Mutex.

// Used only in the case of a Futex.
static inline bool mylock_hold(lock_t *l) {
  return futex_mutex_hold(&l->f_m);
}

// Used only in the case of a Futex.
static inline bool mylock_subscribe(lock_t *l) {
  return futex_mutex_subscribe(&l->f_m);
}

// Lock wrapper class.
class mylock_raii {
  lock_t *mylock;
public:
  mylock_raii(lock_t *mylock) : mylock(mylock) {
    switch (mode) {
    case MODE_PTHREAD_MUTEX:
      pthread_mutex_lock(&mylock->pt_m);
      break;
    case MODE_TSX:
      futex_mutex_lock(&mylock->f_m);
      break;
    default:
      abort();
    }
  }
  ~mylock_raii() {
    switch (mode) {
    case MODE_PTHREAD_MUTEX:
      pthread_mutex_unlock(&mylock->pt_m);
      break;
    case MODE_TSX:
      futex_mutex_unlock(&mylock->f_m);
      break;
    default:
      abort();
    }
  }
};

extern bool use_transactions;
extern bool do_predo;

#define XABORT_LOCK_HELD 9

extern bool has_tsx;
#define have_rtm (has_tsx && use_transactions)

//#define DO_FAILED_COUNTS
#ifdef DO_FAILED_COUNTS
struct atomic_stats_s {
  uint64_t atomic_count __attribute__((aligned(64)));
  uint64_t locked_count;
};
extern struct atomic_stats_s atomic_stats;

struct failed_counts_s {
  const char *name;
  unsigned int  code;
  uint64_t count;
};
extern lock_t  failed_counts_mutex;
static const int max_failed_counts = 100;
extern int    failed_counts_n;
extern struct failed_counts_s failed_counts [max_failed_counts];
#endif

//#define LATE_LOCK_SUBSCRIPTION

template<typename ReturnType, typename... Arguments>
static inline ReturnType atomically(lock_t *mylock,
				    const char *name __attribute__((unused)),
			            void (*predo)(Arguments... args),
				    ReturnType (*fun)(Arguments... args),
				    Arguments... args) {
  if (mode == MODE_PTHREAD_MUTEX) {
    mylock_raii m(mylock);
    ReturnType r = fun(args...);
    return r;
  }
#ifdef DO_FAILED_COUNTS
  __sync_fetch_and_add(&atomic_stats.atomic_count, 1);
#endif
  unsigned int xr = 0xfffffff2;
  if (have_rtm) {
    // Be a little optimistic: try to run the function without the predo if we the lock looks good
    if (mylock_subscribe(mylock) == 0) {
      xr = _xbegin();
      if (xr == _XBEGIN_STARTED) {
#ifndef LATE_LOCK_SUBSCRIPTION
	if (mylock_subscribe(mylock)) _xabort(XABORT_LOCK_HELD);
#endif
	ReturnType r = fun(args...);
#ifdef LATE_LOCK_SUBSCRIPTION
	if (mylock_subscribe(mylock)) _xabort(XABORT_LOCK_HELD);
#endif
	_xend();
	return r;
      }
    }

    int count = 0;
    while (count < 10) {
      mylock_hold(mylock);
      if (do_predo) predo(args...);
      while (mylock_hold(mylock)) {
	// If the lock was held for a long time, then do the predo code again.
	if (do_predo) predo(args...);
      }
      xr = _xbegin();
      if (xr == _XBEGIN_STARTED) {
#ifndef LATE_LOCK_SUBSCRIPTION
	if (mylock_subscribe(mylock)) _xabort(XABORT_LOCK_HELD);
#endif
	ReturnType r = fun(args...);
#ifdef LATE_LOCK_SUBSCRIPTION
	if (mylock_subscribe(mylock)) _xabort(XABORT_LOCK_HELD);
#endif
	_xend();
	return r;
      } else if ((xr & _XABORT_EXPLICIT) && (_XABORT_CODE(xr) == XABORT_LOCK_HELD)) {
	count = 0; // reset the counter if we had an explicit lock contention abort.
	continue;
      } else {
	count++;
	//int backoff = (prandnum()%16) << count;
	for (int i = 1; i < (1<<count); i++) {
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
#ifdef DO_FAILED_COUNTS
  {
    mylock_raii m(&failed_counts_mutex);
    for (int i = 0; i < failed_counts_n; i++) {
      if (failed_counts[i].name == name && failed_counts[i].code == xr) {
	failed_counts[i].count++;
	goto didit;
      }
    }
    bassert(failed_counts_n < max_failed_counts);
    failed_counts[failed_counts_n++] = (struct failed_counts_s){name, xr, 1};
 didit:;
  }

   __sync_fetch_and_add(&atomic_stats.locked_count, 1);
#endif
  if (do_predo) predo(args...);
  mylock_raii mr(mylock);
  ReturnType r = fun(args...);
  return r;
}

template<typename ReturnType, typename... Arguments>
static inline ReturnType atomically2(lock_t *lock0,
				     lock_t *lock1,
				     const char *name __attribute__((unused)),
				     void (*predo)(Arguments... args),
				     ReturnType (*fun)(Arguments... args),
				     Arguments... args) {
  if (mode == MODE_PTHREAD_MUTEX) {
    mylock_raii m0(lock0);
    mylock_raii m1(lock1);
    ReturnType r = fun(args...);
    return r;
  }
#ifdef DO_FAILED_COUNTS
  __sync_fetch_and_add(&atomic_stats.atomic_count, 1);
#endif
  unsigned int xr = 0xfffffff2;
  if (have_rtm) {
    // Be a little optimistic: try to run the function without the predo if we the lock looks good
    if (mylock_subscribe(lock0) == 0 &&
	mylock_subscribe(lock1) == 0) {
      xr = _xbegin();
      if (xr == _XBEGIN_STARTED) {
#ifndef LATE_LOCK_SUBSCRIPTION
	if (mylock_subscribe(lock0) || mylock_subscribe(lock1)) _xabort(XABORT_LOCK_HELD);
#endif
	ReturnType r = fun(args...);
#ifdef LATE_LOCK_SUBSCRIPTION
	if (mylock_subscribe(lock0) || mylock_subscribe(lock1)) _xabort(XABORT_LOCK_HELD);
#endif
	_xend();
	return r;
      }
    }

    int count = 0;
    while (count < 10) {
      mylock_hold(lock0);
      mylock_hold(lock1);
      if (do_predo) predo(args...);
      while (mylock_hold(lock0) || mylock_hold(lock1)) {
	// If the lock was held for a long time, then do the predo code again.
	if (do_predo) predo(args...);
      }
      xr = _xbegin();
      if (xr == _XBEGIN_STARTED) {
#ifndef LATE_LOCK_SUBSCRIPTION
	if (mylock_subscribe(lock0) || mylock_subscribe(lock1)) _xabort(XABORT_LOCK_HELD);
#endif
	ReturnType r = fun(args...);
#ifdef LATE_LOCK_SUBSCRIPTION
	if (mylock_subscribe(lock0) || mylock_subscribe(lock1)) _xabort(XABORT_LOCK_HELD);
#endif
	_xend();
	return r;
      } else if ((xr & _XABORT_EXPLICIT) && (_XABORT_CODE(xr) == XABORT_LOCK_HELD)) {
	count = 0; // reset the counter if we had an explicit lock contention abort.
	continue;
      } else {
	count++;
	//int backoff = (prandnum()%16) << count;
	for (int i = 1; i < (1<<count); i++) {
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
#ifdef DO_FAILED_COUNTS
  {
    mylock_raii m(&failed_counts_mutex);
    for (int i = 0; i < failed_counts_n; i++) {
      if (failed_counts[i].name == name && failed_counts[i].code == xr) {
	failed_counts[i].count++;
	goto didit;
      }
    }
    bassert(failed_counts_n < max_failed_counts);
    failed_counts[failed_counts_n++] = (struct failed_counts_s){name, xr, 1};
 didit:;
  }

   __sync_fetch_and_add(&atomic_stats.locked_count, 1);
#endif
  if (do_predo) predo(args...);
  mylock_raii mr0(lock0);
  mylock_raii mr1(lock1);
  ReturnType r = fun(args...);
  return r;
}

#if 0
struct lock {
  unsigned int l __attribute((aligned(64)));
};
#endif


#define atomic_load(addr) __atomic_load_n(addr, __ATOMIC_CONSUME)
#define atomic_store(addr, val) __atomic_store_n(addr, val, __ATOMIC_RELEASE)

#define prefetch_read(addr) __builtin_prefetch(addr, 0, 3)
#define prefetch_write(addr) __builtin_prefetch(addr, 1, 3)
#define load_and_prefetch_write(addr) ({ __typeof__(*addr) ignore __attribute__((unused)) = atomic_load(addr); prefetch_write(addr); })

static inline void fetch_and_max(uint64_t * ptr, uint64_t val) {
  while (1) {
    uint64_t old = atomic_load(ptr);
    if (val <= old) return;
    if (__sync_bool_compare_and_swap(ptr, old, val)) return;
  }
}

#endif // ATOMICALLY_H
