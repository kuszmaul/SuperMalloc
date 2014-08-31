#include "atomically.h"
#include "bassert.h"
#include "generated_constants.h"
#include "malloc_internal.h"

static struct {
  dynamic_small_bin_info lists;

 // 0 means all pages are full (all the pages are in slot 0).
 // Else 1 means there's a page with 1 free slot, and some page is in slot 1.
 // Else 2 means there's one with 2 free slots, and some page is in slot 2.
 // The full pages are in slot 0, the 1-free are in slot 1, and so forth.  Note
 // that we can have some full pages and have the fullest_offset be nonzero
 // (if not *all* pages are full).
  uint16_t fullest_offset[first_large_bin_number];
} dsbi;

const uint32_t bitmap_n_words = pagesize/64/8; /* 64 its per uint64_t, 8 is the smallest object */ 

struct per_page {
  per_page *next __attribute__((aligned(64)));
  per_page *prev;
  uint64_t inuse_bitmap[bitmap_n_words]; // up to 512 objects (8 bytes per object) per page.  The bit is set if the object is in use.
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

static void verify_small_invariants(void) {
  return;
  for (binnumber_t bin = 0; bin < first_large_bin_number; bin++) {
    uint16_t fullest_off = dsbi.fullest_offset[bin];
    int start       = dynamic_small_bin_offset(bin);
    int opp = static_bin_info[bin].objects_per_page;
    if (fullest_off==0) {
      for (uint16_t i = 1; i <= opp; i++) {
	bassert(dsbi.lists.b[start + i] == NULL);
      }
    } else {
      bassert(fullest_off <= opp);
      bassert(dsbi.lists.b[start + fullest_off] != NULL);
      for (uint16_t i = 1; i < fullest_off; i++) {
	bassert(dsbi.lists.b[start + i] == NULL);
      }
    }
    for (uint16_t i = 0; i <= opp; i++) {
      per_page *prev_pp = NULL;
      for (per_page *pp = dsbi.lists.b[start + i]; pp; pp = pp->next) {
	bassert(prev_pp == pp->prev);
	prev_pp = pp;
	int sum = 0;
	for (uint32_t j = 0; j < bitmap_n_words; j++) {
	  sum += __builtin_popcountl(pp->inuse_bitmap[j]);
	}
	bassert(sum == opp - i);
      }
    }
  }
}

lock small_lock;

struct small_malloc_add_pages_from_chunk_s {
  binnumber_t bin;
  uint32_t dsbi_offset;
  uint32_t o_per_page;
  small_chunk_header *sch;
} __attribute__((aligned(64)));

static void predo_small_malloc_add_pages_from_new_chunk(void *vv) {
  small_malloc_add_pages_from_chunk_s *v = (small_malloc_add_pages_from_chunk_s *)vv;
  binnumber_t bin = v->bin;
  uint32_t dsbi_offset = v->dsbi_offset;
  uint32_t o_per_page  = v->o_per_page;
  small_chunk_header *sch = v->sch;
  per_page *old_h __attribute__((unused)) = atomic_load(&dsbi.lists.b[dsbi_offset + o_per_page]);
  per_page *other_old_h __attribute__((unused)) = atomic_load(&sch->ll[n_pages_used-1].next);
  prefetch_write(&dsbi.lists.b[dsbi_offset + o_per_page]);
  prefetch_write(&sch->ll[n_pages_used-1].next);
  if (dsbi.fullest_offset[bin]) {
    prefetch_write(&dsbi.fullest_offset[bin]);
  }
}

static void do_small_malloc_add_pages_from_new_chunk(void *vv) {
  small_malloc_add_pages_from_chunk_s *v = (small_malloc_add_pages_from_chunk_s *)vv;
  binnumber_t bin = v->bin;
  uint32_t dsbi_offset = v->dsbi_offset;
  uint32_t o_per_page  = v->o_per_page;
  small_chunk_header *sch = v->sch;
  per_page *old_h = dsbi.lists.b[dsbi_offset + o_per_page];
  dsbi.lists.b[dsbi_offset + o_per_page] = &sch->ll[0];
  sch->ll[n_pages_used-1].next = old_h;
  if (dsbi.fullest_offset[bin] == 0) { // must test this again here.
    dsbi.fullest_offset[bin] = o_per_page;
  }
}

struct do_small_malloc_s {
  void *result;
  binnumber_t bin;
  uint32_t dsbi_offset;
  uint32_t o_per_page;
  uint32_t o_size;
} __attribute__((aligned(64)));

static void predo_small_malloc(void* vv) {
  do_small_malloc_s *v = (do_small_malloc_s*)vv;
  binnumber_t bin = v->bin;
  uint32_t dsbi_offset = v->dsbi_offset;
  uint32_t o_per_page  = v->o_per_page;
  uint32_t o_size __attribute__((unused)) = atomic_load(&v->o_size);
  uint32_t fullest = dsbi.fullest_offset[bin]; // we'll want to reread this in the transaction, so let's do it now even without the atomicity.
  if (fullest != 0)  {
    per_page *result_pp = dsbi.lists.b[dsbi_offset + fullest];
    if (result_pp) {
      per_page *next = result_pp->next;
      if (next) {
	per_page *ignore __attribute__((unused)) = atomic_load(&next->prev);
	prefetch_write(&next->prev);
      }
      prefetch_write(&dsbi.lists.b[dsbi_offset + fullest]);
      prefetch_write(&result_pp->next);
      per_page *old_h_below = dsbi.lists.b[dsbi_offset + fullest -1];
      if (old_h_below) {
	per_page *ignore __attribute__((unused)) = atomic_load(&old_h_below->prev);
	prefetch_write(&old_h_below->prev);
      }
      prefetch_write(&dsbi.lists.b[dsbi_offset + fullest -1]);
      prefetch_write(&dsbi.fullest_offset[bin]);
      if (fullest == 0) {
	for (uint32_t new_fullest = 1; new_fullest <= o_per_page; new_fullest++) {
	  if (atomic_load(&dsbi.lists.b[dsbi_offset + new_fullest]))
	    break;
	}
      }
      for (uint32_t w = 0; w < bitmap_n_words; w++) {
	uint64_t bw = result_pp->inuse_bitmap[w];
	if (bw != UINT64_MAX) {
	  prefetch_write(&result_pp->inuse_bitmap[w]);
	}
      }
    }
  }
}  


static void do_small_malloc(void* vv) {
  do_small_malloc_s *v = (do_small_malloc_s*)vv;
  binnumber_t bin = v->bin;
  uint32_t dsbi_offset = v->dsbi_offset;
  uint32_t o_per_page  = v->o_per_page;
  uint32_t o_size = v->o_size;

  uint32_t fullest = dsbi.fullest_offset[bin]; // we'll want to reread this in the transaction, so let's do it now even without the atomicity.
  if (fullest != 0) {
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
      uint64_t bw = result_pp->inuse_bitmap[w];
      if (bw != UINT64_MAX) {
	// Found an empty bit.
	uint64_t bwbar = ~bw;
	int      bit_to_set = __builtin_ctzl(bwbar);
	result_pp->inuse_bitmap[w] |= (1ul<<bit_to_set);

	if (0) printf("result_pp  = %p\n", result_pp);
	if (0) printf("bit_to_set = %d\n", bit_to_set);

	uint64_t chunk_address = ((int64_t)result_pp) & ~(chunksize-1);
	uint64_t wasted_off   = n_pages_wasted*pagesize;
	uint64_t page_num     = (((uint64_t)result_pp)%chunksize)/sizeof(per_page);
	uint64_t page_off     = page_num*pagesize;
	uint64_t obj_off      = (w * 64 + bit_to_set) * o_size;
	v->result = (void*)(chunk_address + wasted_off + page_off + obj_off);
	goto did_set_bitmap;
      }
    }
    abort(); // It's bad if we get here, it means that there was no bit in the bitmap, but the data structure said there should be.
 did_set_bitmap:
    if (0) printf("Did set bitmap, got %p\n", v->result);
  }
}

void* small_malloc(size_t size)
// Effect: Allocate a small object (subpage, class 1 and class 2 are
// treated the same by all the code, it's just the sizes that matter).
// We want to allocate a small object in the fullest possible page.
{
  if (0) printf("small_malloc(%ld)\n", size);
  verify_small_invariants();
  binnumber_t bin = size_2_bin(size);
  //size_t usable_size = bin_2_size(bin);
  bassert(bin < first_large_bin_number);
  uint32_t dsbi_offset = dynamic_small_bin_offset(bin);
  uint32_t o_per_page = static_bin_info[bin].objects_per_page;
  uint32_t o_size     = static_bin_info[bin].object_size;
  bool needed = false;
  while (1) {
    uint32_t fullest = atomic_load(&dsbi.fullest_offset[bin]); // Otherwise it looks racy.
    if (0) printf(" bin=%d off=%d  fullest=%d\n", bin, dsbi_offset, fullest);
    if (fullest==0) {
      if (0) printf("Need a chunk\n");
      needed = true;
      void *chunk = mmap_chunk_aligned_block(1);
      printf("small_map %p\n", chunk);
      bassert(chunk);
      chunk_infos[address_2_chunknumber(chunk)].bin_number = bin;

      small_chunk_header *sch = (small_chunk_header*)chunk;
      for (uint32_t i = 0; i < n_pages_used; i++) { // really ought to git rid of that division.  There's no reason for it, except that I'm trying to keep the code simple for now.
	for (uint32_t w = 0; w < bitmap_n_words; w++) {
	  sch->ll[i].inuse_bitmap[w] = 0;
	}
	sch->ll[i].prev = (i   == 0)              ? NULL : &sch->ll[i-1];
	sch->ll[i].next = (i+1 == n_pages_used)   ? NULL : &sch->ll[i+1];
      }
      struct small_malloc_add_pages_from_chunk_s v = {bin, dsbi_offset, o_per_page, sch};
      atomically(&small_lock.l, predo_small_malloc_add_pages_from_new_chunk, do_small_malloc_add_pages_from_new_chunk, &v);
    }

    if (0 && needed) printf("Chunked\n");
    if (0) printf("There's one somewhere\n");
    

    struct do_small_malloc_s v = {NULL, bin, dsbi_offset, o_per_page, o_size};
    atomically(&small_lock.l, predo_small_malloc, do_small_malloc, &v);

    verify_small_invariants();
    if (v.result) {
      bassert(chunk_infos[address_2_chunknumber(v.result)].bin_number == bin);
      return v.result;
    }
  }
}

struct do_small_free_s {
  binnumber_t bin;
  per_page *pp;
  uint64_t objnum;
  uint32_t dsbi_offset;
  uint32_t o_per_page;
} __attribute__((aligned(64)));

void predo_small_free(void *vv) {
  do_small_free_s   *v = (do_small_free_s*)vv;
  binnumber_t      bin = v->bin;
  per_page         *pp = v->pp;
  uint64_t      objnum = v->objnum;
  uint32_t dsbi_offset = v->dsbi_offset;
  uint32_t o_per_page  = v->o_per_page;
  uint32_t old_count = 0;
  for (uint32_t i = 0; i < bitmap_n_words; i++) old_count += __builtin_popcountl(pp->inuse_bitmap[i]);
  uint64_t bm __attribute__((unused)) = atomic_load(&pp->inuse_bitmap[objnum/64]);
  prefetch_write(&pp->inuse_bitmap[objnum/64]);

  uint32_t old_offset_within = o_per_page - old_count;
  uint32_t old_offset_dsbi = dsbi_offset + old_offset_within;
  uint32_t new_offset = old_offset_dsbi + 1;

  per_page *p_n __attribute__((unused)) = atomic_load(&pp->next);
  per_page *p_p                         = atomic_load(&pp->prev);

  prefetch_write(&pp->next);
  prefetch_write(&pp->prev);
  if (p_p == NULL) {
    prefetch_write(&dsbi.lists.b[old_offset_dsbi]);
  } else {
    prefetch_write(&p_p->next);
  }
  if (p_n != NULL) {
    prefetch_write(&p_n->prev);
  }
  if (old_offset_within == 0
      || (p_n == NULL && dsbi.fullest_offset[bin] == old_offset_within)) {
    prefetch_write(&dsbi.fullest_offset[bin]);
  }
  if (dsbi.lists.b[new_offset]) {
    prefetch_write(&dsbi.lists.b[new_offset]->prev);
  }
  prefetch_write(&dsbi.lists.b[new_offset]);
}

void do_small_free(void *vv) {
  do_small_free_s   *v = (do_small_free_s*)vv;
  binnumber_t      bin = v->bin;
  per_page         *pp = v->pp;
  uint64_t      objnum = v->objnum;
  uint32_t dsbi_offset = v->dsbi_offset;
  uint32_t  o_per_page = v->o_per_page;

  uint32_t old_count = 0;
  for (uint32_t i = 0; i < bitmap_n_words; i++) old_count += __builtin_popcountl(pp->inuse_bitmap[i]);
  // clear the bit.
  bassert(pp->inuse_bitmap[objnum/64] & (1ul << (objnum%64)));
  pp->inuse_bitmap[objnum/64] &= ~ ( 1ul << (objnum%64 ));
  bassert(old_count > 0 && old_count <= o_per_page);

  uint32_t old_offset_within = o_per_page - old_count;
  uint32_t new_offset_within = old_offset_within + 1;
  uint32_t old_offset_dsbi = dsbi_offset + old_offset_within;
  uint32_t new_offset = old_offset_dsbi + 1;

  // remove from old list
  per_page * pp_next = pp->next;  
  per_page * pp_prev = pp->prev;
  if (pp_prev == NULL) {
    bassert(dsbi.lists.b[old_offset_dsbi] == pp);
    dsbi.lists.b[old_offset_dsbi] = pp_next;
  } else {
    pp_prev->next = pp_next;
  }
  if (pp_next != NULL) {
    pp_next->prev = pp_prev;
  }
  // Fix up the old_count
  if (old_offset_within == 0) {
    dsbi.fullest_offset[bin] = new_offset_within;
  } else if (pp_next == NULL && dsbi.fullest_offset[bin] == old_offset_within) {
    dsbi.fullest_offset[bin] = new_offset_within;
  }
  // Add to new list
  pp->prev = NULL;
  pp->next = dsbi.lists.b[new_offset];
  if (dsbi.lists.b[new_offset]) {
    dsbi.lists.b[new_offset]->prev = pp;
  }
  dsbi.lists.b[new_offset] = pp;
}

void small_free(void* p) {
  verify_small_invariants();
  void *chunk = (void*)((uint64_t)p&~(chunksize-1));
  small_chunk_header *sch = (small_chunk_header*)chunk;
  uint64_t page_num = (((uint64_t)p)%chunksize)/pagesize;
  bassert(page_num >= n_pages_wasted);
  chunknumber_t chunk_num  = address_2_chunknumber(p);
  binnumber_t   bin        = chunk_infos[chunk_num].bin_number;
  uint32_t useful_page_num = page_num - n_pages_wasted;
  per_page             *pp = &sch->ll[useful_page_num];
  uint32_t o_size     = static_bin_info[bin].object_size;
  uint64_t         objnum = (((uint64_t)p)%pagesize) / o_size;
  bassert((pp->inuse_bitmap[objnum/64] >> (objnum%64)) & 1);
  uint32_t dsbi_offset = dynamic_small_bin_offset(bin);
  uint32_t o_per_page = static_bin_info[bin].objects_per_page;

  struct do_small_free_s v = {bin, pp, objnum, dsbi_offset, o_per_page};

  // Do this atomically.
  atomically(&small_lock.l, predo_small_free, do_small_free, &v);
  verify_small_invariants();
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

  {
    void *x = small_malloc(2048);
    printf("x (2k)=%p\n", x);
    small_free(x);
  }
  void *x = small_malloc(2048);
  printf("x (2k)=%p\n", x);

  void *y = small_malloc(2048);
  printf("y (2k)=%p\n", y);
  void *z = small_malloc(2048);
  printf("z (2k)=%p\n", z);
  bassert(chunk_infos[address_2_chunknumber(z)].bin_number == size_2_bin(2048));

  for (int i = 0; i < n8; i++) {
    small_free(data8[i]);
  }
  for (int i = 0; i < n16; i++) {
    small_free(data16[i]);
  }
  small_free(x);
  small_free(y);
  small_free(z);
  

}
#endif
