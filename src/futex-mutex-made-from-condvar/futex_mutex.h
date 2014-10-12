#ifndef FUTEX_MUTEX_H
#define FUTEX_MUTEX_H

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// An HTM-friendly futex mutex.

// See futex_mutex.cc for the meaning of these fields.
typedef struct futex_mutex_s {
  volatile int locked;
  volatile uint64_t n_locking;
  volatile uint64_t n_waiting;
  pthread_mutex_t mutex;
  pthread_cond_t  locking, waiting;
} futex_mutex_t;

#define FUTEX_MUTEX_INITIALIZER {0,0,0,PTHREAD_MUTEX_INITIALIZER,PTHREAD_COND_INITIALIZER,PTHREAD_COND_INITIALIZER}
int futex_mutex_lock(futex_mutex_t *m); // return 0 if it's a fast lock, 1 if it's slow.
void futex_mutex_unlock(futex_mutex_t *m);
int futex_mutex_subscribe(futex_mutex_t *m); // (return 0 if it is unlocked)
int futex_mutex_wait(futex_mutex_t *m); // return true if it was a long wait

#ifdef TESTING
void test_futex();
#endif

#ifdef __cplusplus
}
#endif

#endif

