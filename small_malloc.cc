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
  if (0) printf("small_malloc(%ld)\n", size);
  binnumber_t bin = size_2_bin(size);
  //size_t usable_size = bin_2_size(bin);
  bassert(bin < first_large_bin_number);
  int dsbi_offset = dynamic_small_bin_offset(bin);
  uint32_t o_per_page = static_bin_info[bin].objects_per_page;
  uint32_t o_size     = static_bin_info[bin].object_size;
  while (1) {
    int fullest = atomic_load(&dsbi.fullest_offset[bin]); // Otherwise it looks racy.
    if (0) printf(" bin=%d off=%d  fullest=%d\n", bin, dsbi_offset, fullest);
    if (fullest==0) {
      printf("Need a chunk\n");
      void *chunk = mmap_chunk_aligned_block(1);
      bassert(chunk);
      small_chunk_header *sch = (small_chunk_header*)chunk;
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
      if (dsbi.fullest_offset[bin] == 0) { // must test this again here.
	dsbi.fullest_offset[bin] = o_per_page;
      }
      fullest = o_per_page;
      // End of atomically.
    }

    if (0) printf("There's one somewhere\n");
    void *result = NULL;
    // Do this atomically
    fullest = dsbi.fullest_offset[bin]; // we'll want to reread this in the transaction, so let's do it now even without the atomicity.
    if (fullest!=0) {
      per_page *result_pp = dsbi.lists.b[dsbi_offset + fullest];
      bassert(result_pp);
      // update the linked list.
      per_page *next = result_pp->next;
      if (next) {
	next->prev = NULL;
      }
      dsbi.lists.b[dsbi_offset + fullest] = next;

      // Add the item to the next list down.
      
      per_page *old_h_below = dsbi.lists.b[dsbi_offset + fullest -1];
      result_pp->next = old_h_below;
      if (old_h_below) {
	old_h_below->prev = result_pp;
      }
      dsbi.lists.b[dsbi_offset + fullest -1] = result_pp;
      
      // Must also figure out the new fullest.
      if (fullest > 1) {
	dsbi.fullest_offset[bin] = fullest-1;
      } else {
	// It was the last item in the page, so we must look to see if we have any other pages.
	int use_new_fullest = 0;
	for (uint32_t new_fullest = 1; new_fullest <= o_per_page; new_fullest++) {
	  if (dsbi.lists.b[dsbi_offset + new_fullest]) {
	    use_new_fullest = new_fullest;
	    break;
	  }
	}
	dsbi.fullest_offset[bin] = use_new_fullest;
      }

      // Now set the bitmap
      for (uint32_t w = 0; w < bitmap_n_words; w++) {
	uint64_t bw = result_pp->bitmap[w];
	if (bw != UINT64_MAX) {
	  // Found an empty bit.
	  uint64_t bwbar = ~bw;
	  int      bit_to_set = __builtin_ctzl(bwbar);
	  result_pp->bitmap[w] |= (1ul<<bit_to_set);

	  if (0) printf("result_pp  = %p\n", result_pp);
	  if (0) printf("bit_to_set = %d\n", bit_to_set);

	  uint64_t chunk_address = ((int64_t)result_pp) & ~(chunksize-1);
	  uint64_t wasted_off   = n_pages_wasted*pagesize;
	  uint64_t page_num     = (((uint64_t)result_pp)%chunksize)/sizeof(per_page);
	  uint64_t page_off     = page_num*pagesize;
	  uint64_t obj_off      = bit_to_set * o_size;
	  result = (void*)(chunk_address + wasted_off + page_off + obj_off);
	  goto did_set_bitmap;
	}
      }
      abort(); // It's bad if we get here, it means that there was no bit in the bitmap, but the data structure said there should be.
   did_set_bitmap:
      if (0) printf("Did set bitmap, got %p\n", result);
    }
    // End of atomically
    if (result) return result;
  } 
}

#ifdef TESTING
const int n8 = 600000;
static void* data8[n8];
const int n16 = n8/2;
static void* data16[n16];

void test_small_malloc(void) {
  test_small_page_header();

  for (int i = 0; i < n8; i++) {
    data8[i] = small_malloc(8);
  }
  printf("%p ", data8[0]);
  printf("%p\n", data8[n8-1]);

  for (int i = 0; i < n16; i++) {
    data16[i] = small_malloc(16);
  }
  printf("%p ", data16[0]);
  printf("%p\n", data16[n16-1]);

  void *x = small_malloc(2048);
  printf("x (2k)=%p\n", x);
  void *y = small_malloc(2048);
  printf("y (2k)=%p\n", y);
  void *z = small_malloc(2048);
  printf("z (2k)=%p\n", z);

}
#endif
