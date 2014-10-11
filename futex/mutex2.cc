#include <linux/futex.h>
#include <sys/syscall.h>
#include <limits.h>
#include <unistd.h>
#include <immintrin.h>
#include <errno.h>
#include <thread>

// The mutex is 0 if unlocked, otherwise is 1 + 2*number waiting.
typedef volatile int mutex;

static long sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
  return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

static long futex_wait(mutex *addr, int val) {
  return sys_futex((void*)addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0);
}
static long futex_wake1(mutex *addr) {
  return sys_futex((void*)addr, FUTEX_WAKE_PRIVATE, 1,   NULL, NULL, 0);
}

static const int lock_spin_count = 100;
static const int unlock_spin_count = 200;

void mutex_lock(mutex *m) {
  int count = 0;
  while (count < lock_spin_count) {
    int old_c = *m;
    if ((old_c & 1) == 1) {
      // Someone else has the lock, so we are spinning.
      _mm_pause();
      count++;
    } else if (__sync_bool_compare_and_swap(m, old_c, old_c | 1)) {
      // No one else had the lock, and we successfully grabbed it.
      return;
    } else {
      // Someone else modified old_c while we were running.  So we just want to try again, without incrementing count or pausing
      continue;
    }
  }
  
  // We got here without getting the lock, so let's add ourselves to the count.
  __sync_fetch_and_add(m, 2);

  // Now we must wait for the lock to go free.  We'll use the futex, but we'll be opportunistic if value changed.
  while (1) {
    int old_c = *m;
    if ((old_c & 1) == 1) {
      // Someone else has the lock
      futex_wait(m, old_c); // we don't care if the futex fails because old_c changed, we'll just go again anyway.
    } else if (__sync_bool_compare_and_swap(m, old_c, old_c -1)) {
      // No one else had the lock, and we managed to grab it (decrementing by 1 has the effect of subtracting 2 (to indicate that we are no longe rwaiting) and setting the lock bit.
      return;
    } else {
      // No one else had the lock, but someone modified it while we were trying to lock, so just try again without pausing
      continue;
    }
  }
}   
  
void mutex_unlock(mutex *m) {
  while (1) {
    int old_m = *m;
    if (__sync_bool_compare_and_swap(m, old_m, old_m & ~1)) {
      if (old_m == 1) return; // it was just 1, and now it's unlocked with no contention.
      else break;             // someone may be waiting, so we'll have to wake them up.
    }
  }
  // Spin a little, hoping that someone takes the lock.
  for (int i = 0; i < unlock_spin_count; i++) {
    if (*m & 1) return;
    _mm_pause();
  }
  // No one took it, so we have to wake someone up.
  futex_wake1(m);
}

int mutex_subscribe(mutex *m) {
  return *m;
}

bool mutex_wait(mutex *m) {
  for (int count = 0; count < lock_spin_count; count++) {
    if (*m == 0) return false; // it was quick
    _mm_pause();
  }
  // So we resign ourselves to waiting
  __sync_fetch_and_add(m, 2);
  bool futexed = false;
  while (1) {
    int old_c = *m;
    if ((old_c & 1) == 1) {
      // someone else has the lock
      futex_wait(m, old_c); // we don't care if the futex fails because old_c changed, we'll just go again anyway.
      futexed = true;
    } else {
      __sync_fetch_and_add(m, -2);
      return futexed;
    }
  }
}
  
