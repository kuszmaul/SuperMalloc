#include <linux/futex.h>
#include <sys/syscall.h>
#include <limits.h>
#include <unistd.h>
#include <immintrin.h>
#include <errno.h>
#include <thread>

typedef int mutex;



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
  int c;
  for (int i = 0; i < lock_spin_count; i++) {
    c = cmpxchg(m, 0, 1);
    if (c == 0) return;
		
    cpu_relax();
  }

  /* The lock is now contended */
  if (c == 1) c = xchg_32(m, 2);
  
  while (c) {
    /* Wait in the kernel */
    sys_futex(m, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
    c = xchg_32(m, 2);
  }
  return;
}

void mutex_unlock(mutex *m) {
  /* Unlock, and if not contended then exit. */
  if (atomic_load(m) == 2) {
    atomic_store(m, 0);
  } else if (xchg_32(m, 0) == 1) {
    return;
  }

  /* Spin and hope someone takes the lock */
  for (int i = 0; i < unlock_spin_count; i++) {
    if (atomic_load(m)) {
      /* Need to set to state 2 because there may be waiters */
      if (cmpxchg(m, 1, 2)) return;
    }
    cpu_relax();
  }
	
  /* We need to wake someone up */
  sys_futex(m, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);

  return;
}

int mutex_trylock(mutex *m)
{
  /* Try to take the lock, if is currently unlocked */
  unsigned c = cmpxchg(m, 0, 1);
  if (c == 0) return 0;
  return EBUSY;
}

mutex m;
void foo() {
  mutex_lock(&m);
  printf("foo sleep\n");
  sleep(2);
  printf("foo slept\n");
  mutex_unlock(&m);
}
  

int main (int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
  std::thread a(foo);
  std::thread b(foo);
  std::thread c(foo);
  a.join();
  b.join();
  c.join();
  return 0;
}
