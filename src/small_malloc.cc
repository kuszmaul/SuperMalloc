#include "atomically.h"
#include "bassert.h"
#include "generated_constants.h"
#include "malloc_internal.h"
#include <sys/mman.h>

lock_t small_locks[first_large_bin_number] = { REPEAT_FOR_SMALL_BINS(LOCK_INITIALIZER) };

static struct {
  dynamic_small_bin_info lists __attribute__((aligned(4096)));

  // 0 means all pages are full (all the pages are in slot 0).
  // Else 1 means there's a page with 1 free slot, and some page is in slot 1.
  // Else 2 means there's one with 2 free slots, and some page is in slot 2.
  // The full pages are in slot 0, the 1-free are in slot 1, and so forth.  Note
  // that we can have some full pages and have the fullest_offset be nonzero
  // (if not *all* pages are full).
  // There's an extra slot for pages that are empty and have been madvised'd to uncommited.

  // For example, if there are 512 objects per folio then the slot 0
  // contains folios with 0 free slots, slot 1 contains folios with 1
  // free slot, ..., slot 512 contains folios with 512 free slots, and
  // slot 513 contains folios with 512 free slots and the folio is not
  // committed to physical memory.  The rationale is that we don't
  // want to constantly pay the cost of madvising an empty page and
  // then touching it again, so we keep at least one page the
  // penultimate slot (number 512), and the madvised' pages in slot
  // 513.

  // We will also eventually provide a way for a separate thread to
  // actually do the madvising so that it won't slow down the thread
  // that is doing malloc.  We'll do that by providing three modes:
  //  [1] The madvise occurs in the free(), which introduces
  //      performance variance to the caller of free().
  //  [2] The madvise occurs in a separate thread which we manage.
  //  [3] The madvise occurs in a thread the user creates using a
  //      function that we create (for this one, we need to provide
  //      some more synchronization so that the user thread can shut
  //      us down if they want to.)

  uint16_t fullest_offset[first_large_bin_number];
} dsbi;

struct small_chunk_header {
  per_folio ll[512];  // This object is 16 pages long, but we don't use that much unless there are lots of folios in a chunk.  We don't use the last the array.  We could get it down to fewer pages if we packed it, but we want this to be
  //                 //  cached-aligned.  Also for larger objects we could use fewer pages since there are fewer objects per folio.
};
//const uint64_t n_pages_wasted = sizeof(small_chunk_header)/pagesize;
//const uint64_t n_pages_used   = (chunksize/pagesize)-n_pages_wasted;

static inline void verify_small_invariants() {
  return;
  {
    mylock_raii mr(&small_locks[27]);
    if (0 && dsbi.fullest_offset[27] == 0) {
      for (unsigned int i = 1; i<static_bin_info[27].objects_per_folio; i++) {
	bassert(dsbi.lists.b27[i]==NULL);
      }
    }
  }
  //  return;
  for (binnumber_t bin = 0; bin < first_large_bin_number; bin++) {
    mylock_raii mr(&small_locks[bin]);
    uint16_t fullest_off = dsbi.fullest_offset[bin];
    int start       = dynamic_small_bin_offset(bin);
    objects_per_folio_t opp = static_bin_info[bin].objects_per_folio;
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
      per_folio *prev_pp = NULL;
      for (per_folio *pp = dsbi.lists.b[start + i]; pp; pp = pp->next) {
	bassert(prev_pp == pp->prev);
	prev_pp = pp;
	int sum = 0;
	for (uint32_t j = 0; j < folio_bitmap_n_words; j++) {
	  sum += __builtin_popcountl(pp->inuse_bitmap[j]);
	}
	bassert(sum == opp - i);
      }
    }
  }
}

static void predo_small_malloc_add_pages_from_new_chunk(binnumber_t bin,
							uint32_t dsbi_offset,
							small_chunk_header *sch) {
  folios_per_chunk_t folios_per_chunk = static_bin_info[bin].folios_per_chunk;
  objects_per_folio_t o_per_folio = static_bin_info[bin].objects_per_folio;
  prefetch_write(&dsbi.lists.b[dsbi_offset + o_per_folio + 1]);
  prefetch_write(&sch->ll[folios_per_chunk-1].next);
  if (dsbi.fullest_offset[bin] == 0) {
    prefetch_write(&dsbi.fullest_offset[bin]);
  }
}

static bool do_small_malloc_add_pages_from_new_chunk(binnumber_t bin,
						     uint32_t dsbi_offset,
						     small_chunk_header *sch) {
  folios_per_chunk_t folios_per_chunk = static_bin_info[bin].folios_per_chunk;
  objects_per_folio_t o_per_folio = static_bin_info[bin].objects_per_folio;
  // The "+ 1" in the lines below is to arrange to add the new folkos
  // to the madvise_done list.  Initially, those folios are
  // uncommitted.
  per_folio *old_h = dsbi.lists.b[dsbi_offset + o_per_folio + 1];
  dsbi.lists.b[dsbi_offset + o_per_folio + 1] = &sch->ll[0];
  sch->ll[folios_per_chunk-1].next = old_h;
  if (dsbi.fullest_offset[bin] == 0) { // must test this again here.
    // Even if the fullest slot is actually in o_per_folio+1, we say it's in o_per_folio.
    dsbi.fullest_offset[bin] = o_per_folio;
  }
  return true; // cannot have the return type with void, since atomically wants to store the return type and then return it.
}

static void predo_small_malloc(binnumber_t bin,
			       uint32_t dsbi_offset,
			       uint32_t o_size __attribute__((unused))) {
  uint32_t fullest = atomic_load(&dsbi.fullest_offset[bin]);
  if (fullest == 0) return; // A chunk must be allocated.
  uint16_t o_per_folio = static_bin_info[bin].objects_per_folio;
  uint32_t fetch_offset = fullest;
  per_folio *result_pp = atomic_load(&dsbi.lists.b[dsbi_offset + fetch_offset]);
  if (result_pp == NULL) {
    if (fullest == o_per_folio) {
      fetch_offset++;
      result_pp = atomic_load(&dsbi.lists.b[dsbi_offset + fetch_offset]);
    }
    if (result_pp == NULL) {
      return; // Can happen only because predo isn't done atomically.
    }
  }
  prefetch_write(&dsbi.lists.b[dsbi_offset + fetch_offset]); // previously fetched, so just make it writeable

  per_folio *next = result_pp->next;
  if (next) {
    load_and_prefetch_write(&next->prev);
  }
  per_folio *old_h_below = atomic_load(&dsbi.lists.b[dsbi_offset + fullest -1]);
  prefetch_write(&dsbi.lists.b[dsbi_offset + fullest -1]); // previously fetched
  if (old_h_below) {
    load_and_prefetch_write(&old_h_below->prev);
  }

  // set the new fullest
  prefetch_write(&dsbi.fullest_offset[bin]);            // previously fetched

  if (fullest <= 1) {
    for (uint32_t new_fullest = 1; new_fullest <= o_per_folio; new_fullest++) {
      if (atomic_load(&dsbi.lists.b[dsbi_offset + new_fullest])) {
	break;
      }
    }
  }

  // prefetch the bitmap
  for (uint32_t w = 0; w < ceil(o_per_folio, 64); w++) {
    uint64_t bw = atomic_load(&result_pp->inuse_bitmap[w]);
    if (bw != UINT64_MAX) {
      prefetch_write(&result_pp->inuse_bitmap[w]);
      break;
    }
  }
}  


static void* do_small_malloc(binnumber_t bin,
			     uint32_t dsbi_offset,
			     uint32_t o_size)
// Effect: If there is one get an object out of the fullest nonempty page, and return it. 
//    If there is no such object return NULL.
//    (Previously, we made sure there was something in a nonempty page, but
//    another thread may have grabbed it.)
{

  uint32_t fullest = dsbi.fullest_offset[bin];
  if (fullest == 0) return NULL; // Indicating that a chunk must be allocated.

  uint16_t o_per_folio = static_bin_info[bin].objects_per_folio;
  uint32_t fetch_offset = fullest;
  per_folio *result_pp = dsbi.lists.b[dsbi_offset + fetch_offset];
  if (fullest == o_per_folio && result_pp == NULL) {
    // Special case, get stuff from the end.
    fetch_offset++;
    result_pp = dsbi.lists.b[dsbi_offset + fetch_offset];
  }

  bassert(result_pp);
  // update the linked list.
  per_folio *next = result_pp->next;

  // When I did a study to try to figure out where most of the
  // transaction conflicts occure, it was here: this line is causing
  // most of the trouble because the fullest slot doesn't move much.
  dsbi.lists.b[dsbi_offset + fetch_offset] = next;

  if (next) {
    next->prev = NULL;
  }
  
  // Add the item to the next list down.
  
  per_folio *old_h_below = dsbi.lists.b[dsbi_offset + fullest -1];
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
    for (uint32_t new_fullest = 1; new_fullest < o_per_folio+2u; new_fullest++) {
      if (dsbi.lists.b[dsbi_offset + new_fullest]) {
	// If the new fullest is the madvise-done pages then pretend
	// that the fullest one is the madvise_needed slot.
	if (new_fullest == o_per_folio+1u) new_fullest = o_per_folio;
	use_new_fullest = new_fullest;
	break;
      }
    }
    dsbi.fullest_offset[bin] = use_new_fullest;
  }

  // Now set the bitmap
  uint32_t w_max = ceil(static_bin_info[bin].objects_per_folio, 64);
  for (uint32_t w = 0; w < w_max; w++) {
    uint64_t bw = result_pp->inuse_bitmap[w];
    if (bw != UINT64_MAX) {
      // Found an empty bit.
      uint64_t bwbar = ~bw;
      int      bit_to_set = __builtin_ctzl(bwbar);
      result_pp->inuse_bitmap[w] = bw | (1ul<<bit_to_set);

      if (0) printf("result_pp  = %p\n", result_pp);
      if (0) printf("bit_to_set = %d\n", bit_to_set);

      uint64_t chunk_address = reinterpret_cast<uint64_t>(address_2_chunkaddress(result_pp));
      uint64_t wasted_off   = static_bin_info[bin].overhead_pages_per_chunk * pagesize;
      uint64_t folio_num     = offset_in_chunk(result_pp)/sizeof(per_folio);
      uint64_t folio_size   = static_bin_info[bin].folio_size;
      uint64_t folio_off     = folio_num * folio_size;
      uint64_t obj_off      = (w * 64 + bit_to_set) * o_size;
      return reinterpret_cast<void*>(chunk_address + wasted_off + folio_off + obj_off);
    }
  }
  abort(); // It's bad if we get here, it means that there was no bit in the bitmap, but the data structure said there should be.
}

//#define MICROTIMING

#ifdef MICROTIMING
#define WHEN_MICROTIMING(x) x
#else
#define WHEN_MICROTIMING(x) (void)(0)
#endif

#ifdef MICROTIMING
static uint64_t rdtsc()
{
  uint32_t hi, lo;

  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return (((uint64_t)lo) | (((uint64_t)hi) << 32));
}

__thread uint64_t clocks_spent_in_early_small_malloc = 0;
__thread uint64_t clocks_spent_initializing_small_chunks = 0;
__thread uint64_t clocks_spent_in_do_small_malloc = 0;
#endif

void* small_malloc(binnumber_t bin)
// Effect: Allocate a small object (all the small sizes are
//  treated the same by all this code.)
//  Allocate a small object in the fullest possible page.
{
  WHEN_MICROTIMING(uint64_t start_small_malloc = rdtsc());
  verify_small_invariants();
  bin_stats_note_malloc(bin);
  //size_t usable_size = bin_2_size(bin);
  bassert(bin < first_large_bin_number);
  uint32_t dsbi_offset = dynamic_small_bin_offset(bin);
  objects_per_folio_t o_per_folio = static_bin_info[bin].objects_per_folio;
  uint32_t o_size     = static_bin_info[bin].object_size;
  uint16_t folios_per_chunk = static_bin_info[bin].folios_per_chunk;
  while (1) {
    WHEN_MICROTIMING(
	uint64_t end_early_small_malloc = rdtsc();
	clocks_spent_in_early_small_malloc += end_early_small_malloc - start_small_malloc
		     );
    uint32_t fullest = atomic_load(&dsbi.fullest_offset[bin]); // Otherwise it looks racy.
    if (0) printf(" bin=%d off=%d  fullest=%d\n", bin, dsbi_offset, fullest);
    if (fullest==0) {
      if (0) printf("Need a chunk\n");
      void *chunk = mmap_chunk_aligned_block(1);
      if (chunk == NULL) return NULL;
      bin_and_size_t b_and_s = bin_and_size_to_bin_and_size(bin, 0);
      bassert(b_and_s != 0);
      chunk_infos[address_2_chunknumber(chunk)].bin_and_size = b_and_s;

      small_chunk_header *sch = (small_chunk_header*)chunk;
      for (uint32_t i = 0; i < folios_per_chunk; i++) {
	for (uint32_t w = 0; w < ceil(o_per_folio, 64); w++) {
	  sch->ll[i].inuse_bitmap[w] = 0;
	}
	sch->ll[i].prev = (i   == 0)                ? NULL : &sch->ll[i-1];
	sch->ll[i].next = (i+1 == folios_per_chunk) ? NULL : &sch->ll[i+1];
      }
      atomically(&small_locks[bin], "small_malloc_add_pages_from_new_chunk",
		 predo_small_malloc_add_pages_from_new_chunk,
		 do_small_malloc_add_pages_from_new_chunk,
		 bin, dsbi_offset, sch);
    }

    verify_small_invariants();

    WHEN_MICROTIMING(
	uint64_t start_do_small_malloc = rdtsc();
	clocks_spent_initializing_small_chunks += start_do_small_malloc - end_early_small_malloc
		     );
    void *result = atomically(&small_locks[bin], "small_malloc",
			      predo_small_malloc, do_small_malloc,
			      bin, dsbi_offset, o_size);
    verify_small_invariants();
    WHEN_MICROTIMING(
      uint64_t end_do_small_malloc = rdtsc();
      clocks_spent_in_do_small_malloc += end_do_small_malloc - start_do_small_malloc;
      start_small_malloc = end_do_small_malloc // so that the subtraction on the next iteration of the loop will work.
		     );
    if (result) {
      bassert(bin_from_bin_and_size(chunk_infos[address_2_chunknumber(result)].bin_and_size) == bin);
      return result;
    }
  }
}

// We want this timing especially when not in test code.
extern "C" void time_small_malloc(void) {
  // measure the time to do small mallocs.
  const int ncalls = 10000000;
  void **array = new void* [ncalls];
  // Prefill the array so that we don't measure the cost of those page faults.
  for (int i = 0; i < ncalls; i++) {
    array[i] = NULL;
  }
    

  struct timespec start,end;

  clock_gettime(CLOCK_MONOTONIC, &start);
  WHEN_MICROTIMING(uint64_t clocks_in_small_malloc = 0);
  for (int i = 0; i < ncalls; i++) {
    WHEN_MICROTIMING(uint64_t start_rdtsc = rdtsc());
    void *n = small_malloc(0); // bin 0 is the smallest size
    WHEN_MICROTIMING(uint64_t end_rdtsc = rdtsc();
		     clocks_in_small_malloc += end_rdtsc - start_rdtsc);
    array[i] = n;
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  //printf("start=%ld.%09ld\n", start.tv_sec, start.tv_nsec);
  //printf("end  =%ld.%09ld\n", end.tv_sec,   end.tv_nsec);
  //printf("tdiff=%0.9f\n", tdiff(&start, &end));
  printf("%fns/small_malloc\n", tdiff(&start, &end)*1e9/ncalls);
  WHEN_MICROTIMING( ({
      printf("%5.1f clocks/small_malloc\n", clocks_in_small_malloc/(double)ncalls);
      printf("%5.1f clocks/small_malloc spent early small malloc\n", clocks_spent_in_early_small_malloc/(double)ncalls);
      printf("%5.1f clocks/small_malloc spent initializing small chunks\n", clocks_spent_initializing_small_chunks/(double)ncalls);
      printf("%5.1f clocks/small_malloc spent in do_small_malloc\n", clocks_spent_in_do_small_malloc/(double)ncalls);
      printf("%5.1f clocks/small_malloc unaccounted for\n",
	     (clocks_in_small_malloc - clocks_spent_in_early_small_malloc - clocks_spent_initializing_small_chunks - clocks_spent_in_do_small_malloc)/(double)ncalls);
      }));

  for (int i = 0; i < ncalls; i++) {
    free(array[i]);
  }
  delete [] array;
}

static void predo_small_free(binnumber_t bin,
			     per_folio *pp,
			     uint64_t objnum,
			     uint32_t dsbi_offset) {
  uint16_t o_per_folio = static_bin_info[bin].objects_per_folio;
  uint32_t old_count = 0;
  uint32_t imax = ceil(static_bin_info[bin].objects_per_folio, 64);
  for (uint32_t i = 0; i < imax; i++) old_count += __builtin_popcountl(atomic_load(&pp->inuse_bitmap[i]));
  // prefetch for clearing the bit.  We know it was just loaded, so we don't have to load it again.
  bassert(objnum/64 < imax);
  prefetch_write(&pp->inuse_bitmap[objnum/64]);

  uint32_t old_offset_within = o_per_folio - old_count;
  uint32_t old_offset_dsbi = dsbi_offset + old_offset_within;
  uint32_t new_offset = old_offset_dsbi + 1;

  per_folio *pp_next = atomic_load(&pp->next);
  per_folio *pp_prev = atomic_load(&pp->prev);

  prefetch_write(&pp->next);
  prefetch_write(&pp->prev);
  if (pp_prev == NULL) {
    load_and_prefetch_write(&dsbi.lists.b[old_offset_dsbi]);
  } else {
    load_and_prefetch_write(&pp_prev->next);
  }
  if (pp_next != NULL) {
    load_and_prefetch_write(&pp_next->prev);
  }
  // prefetch for fixing up the count
  uint32_t fullest_off = atomic_load(&dsbi.fullest_offset[bin]);
  if (old_offset_within == 0
      || (pp_next == NULL && fullest_off == old_offset_within)) {
    prefetch_write(&dsbi.fullest_offset[bin]);
  }
  per_folio *new_next = atomic_load(&dsbi.lists.b[new_offset]);
  if (new_next) {
    load_and_prefetch_write(&new_next->prev);
  }
  prefetch_write(&dsbi.lists.b[new_offset]);
}

static per_folio* do_small_free(binnumber_t bin,
				per_folio *pp,
				uint64_t objnum,
				uint32_t dsbi_offset)
// Effect: Free the object specified by objnum and pp (that is the
// objnum'th object in the folio corresponding to pp).  Returns NULL
// or else a pointer to a folio that should be freed.
{
  uint16_t o_per_folio = static_bin_info[bin].objects_per_folio;
  uint32_t old_count = 0;
  uint32_t imax = ceil(static_bin_info[bin].objects_per_folio, 64);
  for (uint32_t i = 0; i < imax; i++) old_count += __builtin_popcountl(pp->inuse_bitmap[i]);
  // clear the bit.
  uint64_t old_bits = pp->inuse_bitmap[objnum/64];
  bassert(old_bits & (1ul << (objnum%64)));
  pp->inuse_bitmap[objnum/64] = old_bits & ~ ( 1ul << (objnum%64 ));
  if (IS_TESTING) bassert(old_count > 0 && old_count <= o_per_folio);

  uint32_t old_offset_within = o_per_folio - old_count;
  uint32_t new_offset_within = old_offset_within + 1;
  uint32_t old_offset_dsbi = dsbi_offset + old_offset_within;
  uint32_t new_offset = old_offset_dsbi + 1;

  // remove from old list
  per_folio * pp_next = pp->next;  
  per_folio * pp_prev = pp->prev;
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
  bassert(new_offset < dsbi_offset + o_per_folio + 1);
  if (new_offset != dsbi_offset + o_per_folio
      || dsbi.lists.b[new_offset] == NULL) {
    // Don't madvise the folio, since either it's not empty or there are no folios in the empty slot.
    // Even if the folio is empty, we want to keep one folio around without madvising() it
    //  in order to have some hysteresis in the madvise()/commit cycle.
    per_folio *new_next = dsbi.lists.b[new_offset];
    pp->prev = NULL;
    pp->next = new_next;
    if (new_next) {
      new_next->prev = pp;
    }
    dsbi.lists.b[new_offset] = pp;
    return NULL;
  } else {
    // Ask the caller madvise the folio (by returning the pp) and add
    // it to the slot later.
    //
    // The fullest_offset is still correct, because we do this only if
    // there is something in the new_offset.
    return pp;
  }
}

void predo_small_free_post_madvise(per_folio * pp, uint32_t total_dsbi_offset) {
  per_folio * new_next = atomic_load(&dsbi.lists.b[total_dsbi_offset]);
  prefetch_write(&pp->prev);
  prefetch_write(&pp->next);
  if (new_next) {
    load_and_prefetch_write(&new_next->prev);
  }
  prefetch_write(&dsbi.lists.b[total_dsbi_offset]);
}

bool small_free_post_madvise(per_folio * pp, uint32_t total_dsbi_offset)
// Effect: After calling madvise to clear a folio, put the folio into the free list.
//  The pp is a per-folio linked-list element stored at the beginning of the chunk.
//  The total_dsbi_offset is the offset that corresponds to the list of completely
//  free folios.
{
  per_folio * new_next = dsbi.lists.b[total_dsbi_offset];
  pp->prev = NULL;
  pp->next = new_next;
  if (new_next) {
    new_next->prev = pp;
  }
  dsbi.lists.b[total_dsbi_offset] = pp;
  return true; // cannot return void from a templated function.
}

void small_free(void* p) {
  verify_small_invariants();
  void *chunk = address_2_chunkaddress(p);
  small_chunk_header *sch = reinterpret_cast<small_chunk_header*>(chunk);
  chunknumber_t chunk_num  = address_2_chunknumber(p);
  bin_and_size_t b_and_s   = chunk_infos[chunk_num].bin_and_size;
  bassert(b_and_s != 0);
  binnumber_t   bin        = bin_from_bin_and_size(b_and_s);
  uint64_t wasted_offset =   static_bin_info[bin].overhead_pages_per_chunk * pagesize;
  uint64_t useful_offset =   offset_in_chunk(p) - wasted_offset;
  bassert(reinterpret_cast<uint64_t>(p) >= wasted_offset);
  uint32_t       folio_num = divide_offset_by_foliosize(useful_offset, bin);
  per_folio            *pp = &sch->ll[folio_num];
  uint32_t folio_size      = static_bin_info[bin].folio_size;
  uint32_t offset_in_folio = useful_offset - folio_num * folio_size;
  uint64_t        objnum   = divide_offset_by_objsize(offset_in_folio, bin);
  if (IS_TESTING) {
    uint32_t o_size     = static_bin_info[bin].object_size;
    uint64_t       objnum2 = offset_in_folio / o_size;
    bassert(objnum == objnum2);
  }
  if (IS_TESTING) bassert((pp->inuse_bitmap[objnum/64] >> (objnum%64)) & 1);
  uint32_t dsbi_offset = dynamic_small_bin_offset(bin);

  per_folio *madvise_me = atomically(&small_locks[bin], "small_free",
				     predo_small_free, do_small_free,
				     bin, pp, objnum, dsbi_offset);
  if (madvise_me) {
    // We are the only one that holds this page (it is empty, so no
    // other thread could free an object into it, and we kept it out
    // of the dsbi lists, so no other thread can try to allocate out
    // of it.)
    bassert(madvise_me == pp);
    uint64_t madvise_address = (chunk_num * chunksize) + wasted_offset + folio_num * folio_size;
    madvise(reinterpret_cast<void*>(madvise_address), folio_size, MADV_DONTNEED);
    // Now put it back into the list.
    // Doing this will not change the fullest offset, since this is fully empty.
    // Cannot quite do this with a compare-and-swap since we have to update dsbi.lists[new_offset] as well as the prev pointer
    // in whatever is there.
    atomically(&small_locks[bin], "small_free_post_madvise",
	       predo_small_free_post_madvise, small_free_post_madvise,
	       pp, dsbi_offset + static_bin_info[bin].objects_per_folio + 1);
  }
  bin_stats_note_free(bin);
  verify_small_invariants();
}

#ifdef TESTING
static void test_bin_27() {
  static const int max_n_objects = 256;
  static void *allocated[max_n_objects];
  static int32_t folio_numbers[max_n_objects];
  static per_folio *pps[max_n_objects];
  static int32_t object_numbers_in_folio[max_n_objects];
  for (int n_objects = 1; n_objects <= max_n_objects; n_objects++) {
    for (int objnum = 0; objnum < n_objects; objnum++) {
      const binnumber_t bin = 27;
      allocated[objnum] = small_malloc(bin);
      int32_t wasted_offset = static_bin_info[bin].overhead_pages_per_chunk * pagesize;
      int32_t useful_offset = offset_in_chunk(allocated[objnum]) - wasted_offset;
      folio_numbers[objnum]  = useful_offset / static_bin_info[bin].folio_size;
      object_numbers_in_folio[objnum] = (useful_offset - folio_numbers[objnum] * static_bin_info[bin].folio_size) / static_bin_info[bin].object_size;
      small_chunk_header *sch = reinterpret_cast<small_chunk_header*>(address_2_chunkaddress(allocated[objnum]));
      pps[objnum] = &sch->ll[folio_numbers[objnum]];
      bassert(object_numbers_in_folio[objnum] < 64);
      bassert(1 == ((pps[objnum]->inuse_bitmap[0] >> object_numbers_in_folio[objnum]) & 1));
    }
    for (int objnum = 0; objnum < n_objects; objnum++) {
      for (int k = 0; k < n_objects; k++) {
	if (k < objnum)
	  bassert(0 == ((pps[k]->inuse_bitmap[0] >> object_numbers_in_folio[k]) & 1));
	else
	  bassert(1 == ((pps[k]->inuse_bitmap[0] >> object_numbers_in_folio[k]) & 1));
      }
      small_free(allocated[objnum]);
      for (int k = 0; k < n_objects; k++) {
	if (k <= objnum)
	  bassert(0 == ((pps[k]->inuse_bitmap[0] >> object_numbers_in_folio[k]) & 1));
	else
	  bassert(1 == ((pps[k]->inuse_bitmap[0] >> object_numbers_in_folio[k]) & 1));
      }
    }
  }
}

const int n8 = 600000;
static void* data8[n8];
const int n16 = n8/2;
static void* data16[n16];

void test_small_malloc(void) {

  // test that the dsbi offsets look reasonable.
  bassert(&dsbi.lists.b0[0] == &dsbi.lists.b[dynamic_small_bin_offset(0)]);
  bassert(&dsbi.lists.b1[0] == &dsbi.lists.b[dynamic_small_bin_offset(1)]);
  bassert(&dsbi.lists.b2[0] == &dsbi.lists.b[dynamic_small_bin_offset(2)]);

  test_bin_27();

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
    void *x = small_malloc(size_2_bin(2048));
    printf("x (2k)=%p\n", x);
    small_free(x);
  }
  void *x = small_malloc(size_2_bin(2048));
  printf("x (2k)=%p\n", x);

  void *y = small_malloc(size_2_bin(2048));
  printf("y (2k)=%p\n", y);
  void *z = small_malloc(size_2_bin(2048));
  printf("z (2k)=%p\n", z);
  bassert(bin_from_bin_and_size(chunk_infos[address_2_chunknumber(z)].bin_and_size) == size_2_bin(2048));

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
