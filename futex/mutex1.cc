#include <linux/futex.h>
#include <sys/syscall.h>
#include <limits.h>
#include <unistd.h>
#include <immintrin.h>
#include <errno.h>

// This mutex is suitable for transactional memory.
// In addition to lock and unlock, it provides two new operations:
//   * subscribe which looks at the mutex state in a way that's guaranteed to cause a transaction to abort if another thread performs a locking operation.
//   * wait which waits until the lock is not held.
// We need two futexes to  make it work, ls (lock_state), and sw (some_waiting)
//
// When locking:   ls 
//
// When unlocking:  if the ls indicates that we are the only waiter, then if (sw) wake(sw);
//                  else if the ls indicates that someone else is waiting, we wake(ls)
//
// When waiting:    if ls is 1, then set sw, and wait(sw).   

typedef struct mutex_s {
  volatile int lock_state; // This is also a futex.  Low order bit indicates unlocked, other bits are how many are waiting.
  volatile int some_waiting; // This is also a futex.  The waiting guys want a broadcast, they aren't going to acquire the lock.
};


static inline int cmpxchg(int *p, int oldv, int newv) {
  return __sync_val_compare_and_swap(p, oldv, newv);
}
static inline int xchg_32(int *p, int v) {
  return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST);
}
static inline void cpu_relax(void) {
  _mm_pause();
}

#define atomic_load(addr) __atomic_load_n(addr, __ATOMIC_CONSUME)
#define atomic_store(addr, val) __atomic_store_n(addr, val, __ATOMIC_RELEASE)

static long sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
  return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

const int lock_spin_count = 100;
const int unlock_spin_count = 200;

void mutex_lock(mutex *m) {
  /* Spin and try to take lock */
  for (int i = 0; i < lock_spin_count; i++) {
    int c = cmpxchg(&m->lock_state, 0, 1);
    if (c == 0) return;
		
    cpu_relax();
  }

  /* The lock is now contended */
  __sync_fetch_and_add(&m->lock_state, 2);
  while (1) {
    int c = m->lock_state;
    if ((c & 1) == 0) {
      if (__sync_bool_compare_and_swap(&m->lock_state, c, c | 1)) {
	// we got the lock
	__sync_fetch_and_add(&m->lock_state, -2);
	return;
      } else {
	continue;
      }
    } else {
      // someone else has the lock
      /* Wait in the kernel */
      sys_futex(&m->lock_state, FUTEX_WAIT_PRIVATE, c, NULL, NULL, 0);
    }
  }
  return;
}

void mutex_unlock(mutex *m) {
  /* Unlock, and if not contended then exit. */
  assert(m->lock_state & 1); // the lock should be held
try_again:
  int s = m->lock_state;
  if (s == 1) {
    if (__sync_bool_compare_and_swap(&m->lock_state, 1, 0)) {
      // I'm the only locker.
      if (m->some_waiting) {
	m->some_waiting = 0; // suppose the lock state went to 1 before this, then whoever does the unlock will wake anyone.
	sys_futex(m->some_waiting, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
      }
      return;
    } else {
      goto try_again;
    }
  } else {
    if (!__sync_bool_compare_and_swap(&m->lock_state, s, s & ~1)) {
      goto try_again;
    }
    /* Spin and hope someone takes the lock */
    for (int i = 0; i < unlock_spin_count; i++) {
      if (m->lock_state & 1) return;
      cpu_relax();
    }

    /* We need to wake someone up */
    sys_futex(m, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
    
    return;
  }
}

int mutex_trylock(mutex *m)
{
  /* Try to take the lock, if is currently unlocked */
  if (m->lock_state == 0
      && __sync_bool_compare_and_swap(&m->lock_state, 0, 1)) {
    return 0;
  } else {
    return EBUSY;
  }
}

int mutex_subscribe(mutex *m) {
  return m->lock_state;
}

// On wait it's    ls: --------
//                 sw: _____/--

int mutex_wait(mutex *m) {
  // Wait for the mutex to be not held.
  // First spin a little.
  for (int i = 0; i < lock_spin_count; i++) {
    if (m->lock_state == 0) return;
    _mm_pause();
  }
  // Now go onto the futex
  while (1) {
    m->some_waiting = 1;
    sys_futex(m->some_waiting, FUTEX_WAIT_PRIVATE, ??, NULL, NULL, 0);
  }
}

#if 0

int futex_wake(int *addr, int count) {
  sys_futex(addr, FUTEX_WAIT, 1, 0, 0, 0);
}

class event
{
 public:
  event () : val (0) { }
  void ev_signal ()
  { ++val;
    futex_wake (&val, INT_MAX); }
  void ev_wait ()
  { futex_wait (&val, val); }
 private:
  int val;
};
#endif
