#include "atomically.h"
#include "bassert.h"
#include "generated_constants.h"
#include "malloc_internal.h"

static struct {
  dynamic_small_bin_info lists;

 // 0 means all pages are full.  Else 1 means there's a page with 1
 // free slot.  Else 2 means there's one with 2 free slots.  The full
 // pages are in slot 0, the 1-free are in slot 1, and so forth.  Note
 // that we can have full pages and have the fullest_offset be nonzero
 // (because not all pages are full).
  int fullest_offset[first_large_bin_number];
} dsbi;

const uint32_t bitmap_n_words = pagesize/64/8; /* 64 its per uint64_t, 8 is the smallest object */ 

struct per_page {
  per_page *next __attribute__((aligned(64)));
  per_page *prev;
  uint64_t bitmap[bitmap_n_words]; // up to 512 objects (8 bytes per object) per page.
};
struct small_chunk_header {
  per_page ll[512];  // This object  exactly 8 pages long.  We don't use the first 8 elements of the array.  We could get it down to 6 pages if we packed it, but weant these things to be cache-aligned.  For objects of size 16 we could get it it down to 4 pages of wastage.
};
const uint64_t n_pages_wasted = sizeof(small_chunk_header)/pagesize;
const uint64_t n_pages_used   = (chunksize/pagesize)-n_pages_wasted;

#ifdef TESTING
void test_small_page_header(void) {
  bassert(sizeof(small_chunk_header) == n_pages_wasted*pagesize);
}
#endif

void* small_malloc(size_t size)
// Effect: Allocate a small object (subpage, class 1 and class 2 are
// treated the same by all the code, it's just the sizes that matter).
// We want to allocate a small object in the fullest possible page.
{
  printf("small_malloc(%ld)\n", size);
  binnumber_t bin = size_2_bin(size);
  //size_t usable_size = bin_2_size(bin);
  bassert(bin < first_large_bin_number);
  int dsbi_offset = dynamic_small_bin_offset(bin);
again:
  // Need some atomicity here.
  int fullest = dsbi.fullest_offset[bin];
  printf(" bin=%d off=%d  fullest=%d\n", bin, dsbi_offset, fullest);
  if (fullest!=0) {
    printf("There's one somewhere\n");
    abort();
  } else {
    printf("Need a page\n");
    void *chunk = mmap_chunk_aligned_block(1);
    bassert(chunk);
    small_chunk_header *sch = (small_chunk_header*)chunk;
    uint32_t o_per_page = static_bin_info[bin].objects_per_page;
    for (uint32_t i = 0; i < n_pages_used; i++) { // really ought to git rid of that division.  There's no reason for it, except that I'm trying to keep the code simple for now.
      for (uint32_t w = 0; w < bitmap_n_words; w++) {
	sch->ll[i].bitmap[w] = 0;
      }
      sch->ll[i].prev = (i   == 0)              ? NULL : &sch->ll[i-1];
      sch->ll[i].next = (i+1 == n_pages_used)   ? NULL : &sch->ll[i+1];
    }
    // Do this atomically
    per_page *old_h = dsbi.lists.b[dsbi_offset + o_per_page]; // really ought to get rid of that cast by forward declaring a per_page in the generated_constants.h file.
    dsbi.lists.b[dsbi_offset + o_per_page] = &sch->ll[0];
    sch->ll[n_pages_used-1].next = old_h;
    abort();
    goto again;
  }
  return 0;
}

#ifdef TESTING
void test_small_malloc(void) {
  test_small_page_header();
  void *x __attribute__((unused)) = small_malloc(8);
}
#endif
