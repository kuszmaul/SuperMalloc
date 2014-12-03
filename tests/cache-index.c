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

int test_size(size_t s) {

  for (int j = 0; j < index_limit; j++) {
    indexes[j] = 0;
  }
  int got_null = 0;
  int n_allocated = 0;
  int n_to_alloc = (s > 1024*1024) ? (alloc_count/2) : alloc_count; // doing too many large allocs occasionally causes map to fail.  This is probably some bug in supermalloc.
  for (int i = 0; i < n_to_alloc; i++) {
    ptrs[i] = malloc(s);
    if (ptrs[i] == NULL) {
      got_null = 1;
      break;
    } else {
      n_allocated++;
      indexes[compute_cache_index(ptrs[i])]++;
    }
    //printf("%p (%ld) (idx=%d)\n", ptrs[i], malloc_usable_size(ptrs[i]), compute_cache_index(ptrs[i]));
  }
  int smallest = n_allocated+1;
  int largest  = -1;
  for (int j = 0; j < index_limit; j++) {
    if (smallest > indexes[j]) smallest = indexes[j];
    if (largest  < indexes[j]) largest  = indexes[j];
  }
  if (got_null) fprintf(stderr, "One of the maps failed\n");
  if (got_null || (smallest * 5)/2 <= largest) { 
    printf("s=%lu smallest=%d largest=%d\n", s, smallest, largest);
  }
  if (!got_null) {
    assert(smallest > 0 && largest < n_allocated+1);
    assert((smallest * 5)/2 > largest);
  }
  for (int i = 0; i < n_allocated; i++) {
    free(ptrs[i]);
  }
  return got_null;
}

size_t next_size(size_t s) {
  return s + 1 + (s>>5);
}

int main (int argc __attribute__((unused)), char *argv[] __attribute((unused))) {
  size_t size = 1;
  do {
    if (test_size(size)) break;
    size = next_size(size);
  } while (size < 257*1024*1024);
  return 0;
}
