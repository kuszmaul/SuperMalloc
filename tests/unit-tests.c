#include <assert.h>
#include <errno.h>
#include "supermalloc.h"
#include "unit-tests.h"
#include "futex_mutex.h"

int main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
  initialize_malloc();
  test_cache_early(); // this test should be done before any mallocs are done
  test_hyperceil();
  test_size_2_bin();
  test_makechunk();
  test_huge_malloc();
  test_large_malloc();
  test_small_malloc();
  test_realloc();
  test_malloc_usable_size();

  {
    errno = 0;
    void *x = malloc(1ul<<50);
    assert(x==0);
    assert(errno=ENOMEM);
  }
  {
    void *y = malloc(10);
    assert(y);
    errno = 0;
    void *x = realloc(y, 1ul<<50);
    assert(x==0);
    assert(errno=ENOMEM);
    free(y);
  }

  test_futex();

  return 0;
}
