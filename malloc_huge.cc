#include <assert.h>

#include "generated_constants.h"

void malloc_huge(size_t size) {
  size_t n_pages = ceil(size, pagesize);
  assert(n_pages==0);
}
