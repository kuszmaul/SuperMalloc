/* Test the cache-index friendlyness */

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "supermalloc.h"

enum { alloc_count = 1024*16 };
void *ptrs[alloc_count];
enum { index_limit = (1<<6) };
int indexes[index_limit];

int compute_cache_index(void *p) {
  size_t a = (size_t)p;
  return (a>>6)%index_limit;
}

void test_size(size_t s) {
  for (int j = 0; j < index_limit; j++) {
    indexes[j] = 0;
  }
  for (int i = 0; i < alloc_count; i++) {
    ptrs[i] = malloc(s);
    indexes[compute_cache_index(ptrs[i])]++;
    //printf("%p (%ld) (idx=%d)\n", ptrs[i], malloc_usable_size(ptrs[i]), compute_cache_index(ptrs[i]));
  }
  int smallest = alloc_count+1;
  int largest  = -1;
  for (int j = 0; j < index_limit; j++) {
    if (smallest > indexes[j]) smallest = indexes[j];
    if (largest  < indexes[j]) largest  = indexes[j];
  }
  printf("s=%lu smallest=%d largest=%d\n", s, smallest, largest);
  assert(smallest > 0 && largest < alloc_count+1);
  if (s < 64) {
    assert(smallest * 4 > largest);
  } else {
    assert(smallest * 2 > largest);
  }
  for (int i = 0; i < alloc_count; i++) {
    free(ptrs[i]);
  }
}

size_t next_size(size_t s) {
 return s + 1 + (s>>4);
}

int main (int argc __attribute__((unused)), char *argv[] __attribute((unused))) {
  size_t size = 1;
  // test_size(99);
  do {
    test_size(size);
    size = next_size(size);
  } while (size < 257*1024*1024);
  return 0;
}
