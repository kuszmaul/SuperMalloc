#ifdef TESTING
#include <stdio.h>
#endif

#include "atomically.h"
#include "malloc_internal.h"
#include "bassert.h"
#include "generated_constants.h"

const binnumber_t n_large_classes = first_huge_bin_number - first_large_bin_number;
extern large_object_list_cell free_large_objects[n_large_classes]; // For each large size, a list (threaded through the chunk headers) of all the free objects of that size.
// Later we'll be a little careful about demapping those large objects (and we'll need to remember which are which, but we may also want thread-specific parts).  For now, just demap them all.

#if 0
void* large_malloc(size_t size)
// Effect: Allocate a large object (page allocated, multiple per chunk)
// Implementation notes: Since it is page allocated, any page is as
//  good as any other.  (Although perhaps someday we ought to prefer
//  to fullest chunk and arrange that full-enough chunks use
//  hugepages.  But for now it doesn't matter which page we use, since
//  for these pages we disable hugepages.)
{
  binnumber_t b = size_2_bin(size);
  assert(b >= first_large_bin_number && b < first_huge_bin_number);

  // This needs to be done atomically (along the successful branch)
  large_object_list_cell *h = free_large_objects[b];
  if (h!=NULL) {
    
  }
}
#endif
