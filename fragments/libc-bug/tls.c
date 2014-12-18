#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
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
  int i;
  for (i = 0; i < N ; i++) {
    int r = pthread_create(&pt[i], NULL, start, NULL);
    assert(r==0);
  }
  for (i = 0; i < N ; i++) {
    void *result;
    int r = pthread_join(pt[i], &result);
    assert(r==0);
  }
  return 0;
}

// My cheap implementation of malloc.
enum { HEAP_SIZE = 100000};
static char data[HEAP_SIZE];
static size_t dataoff = 0;

static bool is_in_range(char *ptr) {
  return ptr >= &data[0] && ptr <  &data[HEAP_SIZE];
}


void* malloc(size_t size) {
  size_t result = __sync_fetch_and_add(&dataoff, size);
  if (result + size < HEAP_SIZE) {
    printf("malloc(%ld)=%p\n", size, &data[result]);
    assert(is_in_range(&data[result]));
    return &data[result];
  } else {
    errno = ENOMEM;
    return 0;
  }
}
void* calloc(size_t number, size_t size) {
  return malloc(number*size);
}
void free(void* ptr) {
  if (ptr == NULL) return;
  if (is_in_range(ptr)) {
    printf("free(%p)\n", ptr);
  } else {
    printf("free passed %p not in range\n", ptr);
    abort();
  }
}
static void *aligned_alloc_internal(size_t alignment, size_t size) {
  size_t p = (unsigned long)(malloc(alignment+size-1));         // allocate enough space.
  if (p==0) return NULL;
  size_t align_up = (p + alignment -1) & (alignment-1);
  fprintf(stderr, "aligned_alloc_internal(%ld, %ld)=%p\n", alignment, size, (void*)align_up);
  return (void*)align_up;
}
void *aligned_alloc(size_t alignment, size_t size) {
  assert((alignment & (alignment - 1)) == 0); // alignment must be a power of two
  assert(size % alignment == 0);              // size must be a multiple of alignment.
  return aligned_alloc_internal(alignment, size);
}
int posix_memalign(void **memptr, size_t alignment, size_t size) {
  if (size==0) {
    *memptr = NULL;                   // size==0 must return NULL or something that can be free()'d
    return 0;
  }
  assert((alignment & (alignment - 1)) == 0); // alignment must be a power of two
  assert(alignment % sizeof(void*) == 0);     // alignment must be a multiple of sizeof(void*)
  void *p = aligned_alloc_internal(alignment, size);
  if (p) {
    *memptr = p;
    return 0;
  } else {
    return ENOMEM;
  }
}
void* realloc(void *p, size_t size) {
  assert(is_in_range(p));
  char *pc = p;
  char *result = malloc(size);
  if (!result) return NULL;
  size_t i;
  // Don't copy off the end of the heap, otherwise, just copy size bytes from p to s (we don't keep track of the usable size, so ...)
  for (i = 0; i < size && pc + i < &data[HEAP_SIZE]; i++) {
    result[i] = pc[i];
  }
  fprintf(stderr, "realloc(%p,%ld)=%p\n", p, size, result);
  return result;
}
