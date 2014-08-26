#ifdef TESTING
#include <stdio.h>
#endif

#include <sys/mman.h>

#include "atomically.h"
#include "bassert.h"
#include "generated_constants.h"
#include "malloc_internal.h"

struct large_object_list_cell {
  union {
    large_object_list_cell *next;
    uint32_t footprint;
  };
};

static const binnumber_t n_large_classes = first_huge_bin_number - first_large_bin_number;
static large_object_list_cell* free_large_objects[n_large_classes]; // For each large size, a list (threaded through the chunk headers) of all the free objects of that size.
// Later we'll be a little careful about demapping those large objects (and we'll need to remember which are which, but we may also want thread-specific parts).  For now, just demap them all.

static unsigned int large_lock = 0;

struct large_malloc_pop_s {
  large_object_list_cell **free_head;
  large_object_list_cell *result;
};
void predo_large_malloc_pop(void *ev) {
  // For the predo, we basically want to look at the free head (and make it writeable) and
  // read the next pointer (but only if the free-head is non-null, since the free-head could
  // have become null by now, and we would need to allocate another chunk.)
  large_malloc_pop_s *e = (large_malloc_pop_s*)ev;
  large_object_list_cell *h = *e->free_head;
  prefetch_write(e->free_head);
  if (h != NULL) {
    e->result = atomic_load(&h->next);
  }
}

void do_large_malloc_pop(void *ev) {
  large_malloc_pop_s *e = (large_malloc_pop_s*)ev;
  large_object_list_cell *h = *e->free_head;
  if (0) printf(" dlmp: h=%p\n", h);
  if(h == NULL) {
    e->result = NULL;
  } else {
    *e->free_head = h->next;
    e->result = h;
  }
}

void* large_malloc(size_t size)
// Effect: Allocate a large object (page allocated, multiple per chunk)
// Implementation notes: Since it is page allocated, any page is as
//  good as any other.  (Although perhaps someday we ought to prefer
//  to fullest chunk and arrange that full-enough chunks use
//  hugepages.  But for now it doesn't matter which page we use, since
//  for these pages we disable hugepages.)
{
  if (0) printf("large_malloc(%ld):\n", size);
  binnumber_t b = size_2_bin(size);
  size_t usable_size = bin_2_size(b);
  bassert(b >= first_large_bin_number);
  bassert(b < first_huge_bin_number);

  large_object_list_cell **free_head = &free_large_objects[b - first_large_bin_number];

  while (1) { // Keep going until we find a free object and return it.
  
    // This needs to be done atomically (along the successful branch).
    // It cannot be done with a compare-and-swap since we read two locations that
    // are visible to other threads (getting h, and getting h->next).
    large_object_list_cell *h = *free_head;
    if (0) printf("h==%p\n", h);
    if (h != NULL) {
      if (0) {
	// This is what we want the atomic code below to do.
	// The atomic code will have to re-read *free_head to get h again.
	*free_head = h->next;
      } else {
	// The strategy for the atomic version is that we set e.result to NULL if the list
	// becomes empty (so that we go around and do chunk allocation again).
	large_malloc_pop_s e = {free_head, 0};
	atomically(&large_lock,
		   predo_large_malloc_pop,
		   do_large_malloc_pop,
		   &e);
	h = e.result;
	if (h==NULL) continue; // Go try again
      }
      // that was the atomic part.
      uint32_t footprint = pagesize*ceil(size, pagesize);
      h->footprint = footprint;
      add_to_footprint(footprint);
      if (0) printf("setting its footprint to %d\n", h->footprint);
      if (0) printf("returning the page corresponding to %p\n", h);
      void* chunk = (void*)(((uint64_t)h)& ~(chunksize-1));
      if (0) printf("chunk=%p\n", chunk);
      large_object_list_cell *chunk_as_list_cell = (large_object_list_cell*)chunk;
      size_t offset = h-chunk_as_list_cell;
      if (0) printf("offset=%ld\n", offset);
      void* address = (void*)((char*)chunk + 2*pagesize + offset * usable_size);
      if (0) printf("result=%p\n", address);
      return address;
    } else {
      // No already free objects.  Get a chunk
      void *chunk = mmap_chunk_aligned_block(1);
      bassert(chunk);
      if (0) printf("chunk=%p\n", chunk);

      if (0) printf("usable_size=%ld\n", usable_size);
      size_t objects_per_chunk = chunksize/usable_size;
      if (0) printf("opce=%ld\n", objects_per_chunk);
      size_t size_of_header = objects_per_chunk * sizeof(large_object_list_cell);
      if (0) printf("soh=%ld\n", size_of_header);

      large_object_list_cell *entry = (large_object_list_cell*)chunk;
      for (size_t i = 0; i+1 < objects_per_chunk; i++) {
	entry[i].next = &entry[i+1];
      }
      chunk_infos[address_2_chunknumber(chunk)].bin_number = b;

      // Do this atomically. 
      if (0) {
	entry[objects_per_chunk-1].next = *free_head;
	*free_head = &entry[0];
      } else {
	while (1) {
	  large_object_list_cell *old_first = *free_head;
	  entry[objects_per_chunk-1].next = old_first;
	  if (__sync_bool_compare_and_swap(free_head,
					   old_first,
					   &entry[0]))
	    break;
	}
      }
    
      if (0) printf("Got object\n");
    }
  }
}

size_t large_footprint(void *p) {
  if (0) printf("large_footprint(%p):\n", p);
  binnumber_t bin = chunk_infos[address_2_chunknumber(p)].bin_number;
  bassert(first_large_bin_number <= bin);
  bassert(bin < first_huge_bin_number);
  size_t usable_size = bin_2_size(bin);
  size_t offset = (uint64_t)p % chunksize;
  size_t objnum = (offset-2*pagesize)/usable_size;
  if (0) printf("objnum %p is in bin %d, usable_size=%ld, objnum=%ld\n", p, bin, usable_size, objnum);
  large_object_list_cell *entries = (large_object_list_cell*)(((uint64_t) p)  & ~(chunksize-1));
  if (0) printf("entries are %p\n", entries);
  uint32_t footprint = entries[objnum].footprint;
  if (0) printf("footprint=%u\n", footprint);
  return footprint;
}

void large_free(void *p) {
  binnumber_t bin = chunk_infos[address_2_chunknumber(p)].bin_number;
  size_t usable_size = bin_2_size(bin);
  madvise(p, usable_size, MADV_DONTNEED);
  size_t offset = (uint64_t)p % chunksize;
  size_t objnum = (offset-2*pagesize)/usable_size;
  large_object_list_cell *entries = (large_object_list_cell*)(((uint64_t) p)  & ~(chunksize-1));
  uint32_t footprint = entries[objnum].footprint;
  add_to_footprint(-(int64_t)footprint);
  large_object_list_cell **h = free_large_objects+ (bin - first_large_bin_number);
  large_object_list_cell *ei = entries+objnum;
  // This part atomic. Can be done with comapre_and_swap
  if (1) {
    ei->next = *h;
    *h = ei;
  } else {
    while (1) {
      large_object_list_cell *first = *h;
      ei->next = first;
      if (__sync_bool_compare_and_swap(h, first, ei)) break;
    }
  }
}


void test_large_malloc(void) {
  int64_t fp = get_footprint();
  {
    void *x = large_malloc(pagesize);
    bassert(x);
    bassert((uint64_t)x % chunksize == 2*pagesize);

    void *y = large_malloc(pagesize);
    bassert(y);
    bassert((uint64_t)y % chunksize == 3*pagesize);

    int64_t fy = large_footprint(y);
    bassert(fy==pagesize);

    bassert(get_footprint() - fp == 2*pagesize);

    large_free(x);
    void *z = large_malloc(pagesize);
    bassert(z==x);

    large_free(z);
    large_free(y);
  }

  bassert(get_footprint() - fp == 0);
  {
    void *x = large_malloc(2*pagesize);
    bassert(x);
    bassert((uint64_t)x % chunksize == (2+0)*pagesize);

    bassert(get_footprint() - fp == 2*pagesize);

    void *y = large_malloc(2*pagesize);
    bassert(y);
    bassert((uint64_t)y % chunksize == (2+2)*pagesize);

    bassert(get_footprint() - fp == 4*pagesize);

    large_free(x);
    void *z = large_malloc(2*pagesize);
    bassert(z==x);

    large_free(z);
    large_free(y);
  }
  bassert(get_footprint() - fp == 0);
  {
    void *x = large_malloc(largest_large);
    bassert(x);
    bassert((uint64_t)x % chunksize == (2+0)*pagesize);

    void *y = large_malloc(largest_large);
    bassert(y);
    bassert((uint64_t)y % chunksize == 2*pagesize + largest_large);

    large_free(x);
    void *z = large_malloc(largest_large);
    bassert(z==x);

    large_free(z);
    large_free(y);

  }
  bassert(get_footprint() - fp == 0);
  {
    size_t s = 100*4096;
    if (0) printf("s=%ld\n", s);

    void *x = large_malloc(s);
    bassert(x);
    bassert((uint64_t)x % chunksize == (2+0)*pagesize);

    bassert(large_footprint(x) == s);

    bassert(get_footprint() - fp == (int64_t)s);

    void *y = large_malloc(s);
    bassert(y);
    bassert((uint64_t)y % chunksize == 2*pagesize + bin_2_size(size_2_bin(s)));

    bassert(large_footprint(y) == s);

    large_free(x);
    large_free(y);
  }
  bassert(get_footprint() - fp == 0);
}
