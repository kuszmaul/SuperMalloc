#ifndef FUTEX_MUTEX_H
#define FUTEX_MUTEX_H

// An HTM-friendly futex mutex.

typedef volatile int futex_mutex __attribute__((aligned(64)));
#define FUTEX_MUTEX_INITIALIZER 0
int futex_mutex_lock(futex_mutex *m);
void futex_mutex_unlock(futex_mutex *m);
int futex_mutex_subscribe(futex_mutex *m); // (return 0 if it is unlocked)
bool futex_mutex_wait(futex_mutex *m); // return true if it was a long wait

#ifdef TESTING
void test_futex();
#endif

#endif

