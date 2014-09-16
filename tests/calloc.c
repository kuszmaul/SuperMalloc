#include <assert.h>
#include <stdio.h>
#include "supermalloc.h"

int main(int argc, char *argv[] __attribute__((unused))) {
  assert(argc == 1);
  size_t max_set = 8*1024*1024;
  for (size_t psize = 128; psize <= 1024ul*1024ul*1024ul; psize *= 2) {
    for (size_t size = psize; size < 2*psize; size += psize/8) {
      char *p = malloc(size);
      for (size_t i = 0; i < size && i < max_set; i++) p[i] = i;
      free(p);
      char *q = calloc(size, 1);
      if (0) {
	// This assertion is not always true, and I think that's OK.
	if (p!=q) printf("Did %p=malloc(%ld) then free(%p), then %p=calloc(%ld, 1)\n", p, size, p, q, size);
	assert(p == q);
      }
      for (size_t i = 0; i < size && i < max_set; i++) assert(q[i] == 0);
      for (size_t i = 0; i < size && i < max_set; i++) q[i] = i;
      free(q);
      char *r = calloc(1, size);
      for (size_t i = 0; i < size && i < max_set; i++) assert(r[i] == 0);
      free(r);
    }
  }
  return 0;
}
