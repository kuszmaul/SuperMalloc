#include <cassert>
#include <pthread.h>
#include <cstdio>
#include <unistd.h>
#include <dlfcn.h>
#include "tls.h"

size_t (*get_size_f)() = NULL;

void* start(void *ignore) {
  //printf("%p %p %p %p\n", tl_current_context, &tl_foo, &tl_bar, &tls_a);
  printf("%ld\n", get_size_f());
  return ignore;
}

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
  void * lib = dlopen("/home/bradley/supermalloc/fragments/libtls.so", RTLD_LAZY);
  assert(lib);
  get_size_f = (size_t(*)())(dlsym(lib, "get_size"));
  assert(get_size_f);
  // context x(argc);
  int N = 5;
  pthread_t pt[N];
  for (int i = 0; i < N ; i++) {
    int r = pthread_create(&pt[i], NULL, start, NULL);
    assert(r==0);
  }
  for (int i = 0; i < N ; i++) {
    void *result;
    int r = pthread_join(pt[i], &result);
    assert(r==0);
  }
  return 0;
}
