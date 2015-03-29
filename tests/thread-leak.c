#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

// Look for a leak of the allocator's internal thread storage.

void* start(void* v) {
  int n_to_malloc = 5;
  int size_to_malloc = 100*4096;
  void **x = malloc(n_to_malloc * sizeof(*x));

  for (int i = 0; i < n_to_malloc; i++) {
    x[i] = malloc(size_to_malloc);
    memset(x[i], 1, size_to_malloc);
  }

  for (int i = 0; i < n_to_malloc; i++) {
    free(x[i]);
  }
  free(x);
  return v;
}

int main(int argc __attribute__((unused)), char *argv[]  __attribute__((unused))) {
  pthread_t th;
  int n_threads = 8;
  long maxrss[n_threads];
  for (int i = 0; i < n_threads; i++) {
    int r = pthread_create(&th, NULL, start, NULL);
    assert(r == 0);
    void *ret;
    r = pthread_join(th, &ret);
    assert(r == 0);
    assert(ret == NULL);
    struct rusage rusage;
    r = getrusage(RUSAGE_SELF, &rusage);
    assert(r == 0);
    maxrss[i] = rusage.ru_maxrss;
    printf("maxrss=%ld\n", rusage.ru_maxrss);
  }
  assert(n_threads>=2);
  assert(maxrss[n_threads-1] == maxrss[n_threads-2]);
  return 0;
}
