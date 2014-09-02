#include <assert.h>
#include "bmalloc.h"

int main(int argc, char *argv[] __attribute__((unused))) {
  assert(argc==1);
  for (size_t psize = 128; psize <= 1024ul*1024ul*1024ul; psize *= 2) {
    for (size_t size = psize; size < 2*psize; size += psize/8) {
      char *p = malloc(size);
      for (size_t i = 0; i < size && i < 128*1024; i++) p[i] = i;
      free(p);
      char *q = calloc(size, 1);
      assert(p == q);
      for (size_t i = 0; i < size && i < 128*1024; i++) assert(q[i] == 0);
      free(q);
    }
  }
  return 0;
}
