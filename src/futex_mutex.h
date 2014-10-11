#ifndef FUTEX_MUTEX_H
#define FUTEX_MUTEX_H

// An HTM-friendly futex mutex.

typedef volatile int futex_mutex_t __attribute__((aligned(64)));
#define FUTEX_MUTEX_INITIALIZER 0
int futex_mutex_lock(futex_mutex_t *m); // return 0 if it's a fast lock, 1 if it's slow.
void futex_mutex_unlock(futex_mutex_t *m);
int futex_mutex_subscribe(futex_mutex_t *m); // (return 0 if it is unlocked)
bool futex_mutex_wait(futex_mutex_t *m); // return true if it was a long wait

#ifdef TESTING
void test_futex();
#endif

#endif

