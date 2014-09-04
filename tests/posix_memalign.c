#include "supermalloc.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#define N (1024*1024)
//static int n=0;
//static void *data[N];

int main(int argc, char *argv[] __attribute__((unused))) {
  assert(argc == 1);

  {
    void *a;
    int r = posix_memalign(&a, sizeof(void*), 1);
    assert(r==0);
    assert(a);
    assert(((uint64_t)a) % sizeof(void*) == 0);
    free(a);
  }
  {
    void *a;
    int r = posix_memalign(&a, sizeof(void*)-1, 1); // too small (not as big as void*) and not power of two
    assert(r==EINVAL);
  }
  {
    void *a;
    int r = posix_memalign(&a, sizeof(void*)/2, 1); // too small, but is a power of two
    assert(r==EINVAL);
  }
  {
    void *a;
    int r = posix_memalign(&a, sizeof(void*)+1, 1); // big enough, not a power of two
    assert(r==EINVAL);
  }
  {
    void *a = (void*)0xDEADBEEF;
    int r = posix_memalign(&a, 16, 0);
    assert(r==0);
    assert(a==NULL);
  }
  for (size_t i = sizeof(void*); i < 1024*1024*1024; i*=2) {
    void *a = (void*)0xDEADBEEF;
    for (int off = -1; off < 2; off++) {
      int r = posix_memalign(&a, i, i + off);
      assert(r==0);
      assert((uint64_t)r % i == 0);
      free(a);
    }
  }
  return 0;
}
