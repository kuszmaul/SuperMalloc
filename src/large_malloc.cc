#ifdef TESTING
#include <stdio.h>
#endif

#include <sys/mman.h>

#include "atomically.h"
#include "bassert.h"
#include "generated_constants.h"
#include "malloc_internal.h"

#ifdef ENABLE_LOG_CHECKING
static void log_command(char command, const void *ptr);
#else
#define log_command(a,b) ((void)0)
#endif

static const binnumber_t n_large_classes = first_huge_bin_number - first_large_bin_number;
static large_object_list_cell* free_large_objects[n_large_classes]; // For each large size, a list (threaded through the chunk headers) of all the free objects of that size.
// Later we'll be a little careful about purging those large objects (and we'll need to remember which are which, but we may also want thread-specific parts).  For now, just purge them all.

static lock_t large_lock = LOCK_INITIALIZER;

void predo_large_malloc_pop(large_object_list_cell **free_head) {
  // For the predo, we basically want to look at the free head (and make it writeable) and
  // read the next pointer (but only if the free-head is non-null, since the free-head could
  // have become null by now, and we would need to allocate another chunk.)
  large_object_list_cell *h = *free_head;
  if (h != NULL) {
    prefetch_write(free_head);
    prefetch_read(h);
  }
}

large_object_list_cell* do_large_malloc_pop(large_object_list_cell **free_head) {
  large_object_list_cell *h = *free_head;
  if (0) printf(" dlmp: h=%p\n", h);
  if(h == NULL) {
    return NULL;
  } else {
    *free_head = h->next;
    return h;
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
  uint32_t footprint = pagesize*ceil(size, pagesize);
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

	h = atomically(&large_lock,
		       "large_malloc_pop",
		       predo_large_malloc_pop,
		       do_large_malloc_pop,
		       free_head);
	if (h==NULL) continue; // Go try again
      }
      // that was the atomic part.
      h->footprint = footprint;
      add_to_footprint(footprint);
      if (0) printf("setting its footprint to %d\n", h->footprint);
      if (0) printf("returning the page corresponding to %p\n", h);
      void* chunk = address_2_chunkaddress(h);
      if (0) printf("chunk=%p\n", chunk);
      large_object_list_cell *chunk_as_list_cell = reinterpret_cast<large_object_list_cell*>(chunk);
      size_t offset = h-chunk_as_list_cell;
      if (0) printf("offset=%ld\n", offset);
      void* address = reinterpret_cast<void*>(reinterpret_cast<char*>(chunk) + offset_of_first_object_in_large_chunk + offset * usable_size);
      bassert(address_2_chunknumber(address)==address_2_chunknumber(chunk));
      if (0) printf("result=%p\n", address);
      bassert(bin_from_bin_and_size(chunk_infos[address_2_chunknumber(address)].bin_and_size) == b);
      log_command('a', address);
      return address;
    } else {
      // No already free objects.  Get a chunk
      void *chunk = mmap_chunk_aligned_block(1);
      bassert(chunk);
      if (0) printf("chunk=%p\n", chunk);

      if (0) printf("usable_size=%ld\n", usable_size);
      size_t objects_per_chunk = (chunksize-offset_of_first_object_in_large_chunk)/usable_size; // Should use magic for this, but it's already got an mmap(), so it doesn't matter
      if (0) printf("opce=%ld\n", objects_per_chunk);
      size_t size_of_header = objects_per_chunk * sizeof(large_object_list_cell);
      if (0) printf("soh=%ld\n", size_of_header);
      bassert(size_of_header <= offset_of_first_object_in_large_chunk);

      large_object_list_cell *entry = (large_object_list_cell*)chunk;
      for (size_t i = 0; i+1 < objects_per_chunk; i++) {
	entry[i].next = &entry[i+1];
      }
      
      bin_and_size_t b_and_s = bin_and_size_to_bin_and_size(b, footprint);
      bassert(b_and_s != 0);
      chunk_infos[address_2_chunknumber(chunk)].bin_and_size = b_and_s;

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
  bin_and_size_t b_and_s = chunk_infos[address_2_chunknumber(p)].bin_and_size;
  bassert(b_and_s != 0);
  binnumber_t bin = bin_from_bin_and_size(b_and_s);
  bassert(first_large_bin_number <= bin);
  bassert(bin < first_huge_bin_number);
  uint64_t usable_size = bin_2_size(bin);
  uint64_t offset = offset_in_chunk(p);
  uint64_t objnum = (offset-offset_of_first_object_in_large_chunk)/usable_size;
  if (0) printf("objnum %p is in bin %d, usable_size=%ld, objnum=%ld\n", p, bin, usable_size, objnum);
  large_object_list_cell *entries = reinterpret_cast<large_object_list_cell*>(address_2_chunkaddress(p));

  if (0) printf("entries are %p\n", entries);
  uint32_t footprint = entries[objnum].footprint;
  if (0) printf("footprint=%u\n", footprint);
  return footprint;
}

void large_free(void *p) {
  log_command('f', p);
  bin_and_size_t b_and_s = chunk_infos[address_2_chunknumber(p)].bin_and_size;
  bassert(b_and_s != 0);
  binnumber_t bin = bin_from_bin_and_size(b_and_s);
  bassert(first_large_bin_number <= bin  && bin < first_huge_bin_number);
  uint64_t usable_size = bin_2_size(bin);
  madvise(p, usable_size, MADV_DONTNEED);
  uint64_t offset = offset_in_chunk(p);
  uint64_t        objnum = divide_offset_by_objsize(offset-offset_of_first_object_in_large_chunk, bin);
  if (IS_TESTING) {
    uint64_t objnum2 = (offset-offset_of_first_object_in_large_chunk)/usable_size;
    bassert(objnum == objnum2);
  }
  large_object_list_cell *entries = reinterpret_cast<large_object_list_cell*>(address_2_chunkaddress(p));
  uint32_t footprint = entries[objnum].footprint;
  add_to_footprint(-static_cast<int64_t>(footprint));
  large_object_list_cell **h = &free_large_objects[bin - first_large_bin_number];
  large_object_list_cell *ei = entries+objnum;
  // This part atomic. Can be done with compare_and_swap
  if (0) {
    ei->next = *h;
    *h = ei;
  } else {
    while (1) {
      large_object_list_cell *first = atomic_load(h);
      ei->next = first;
      if (__sync_bool_compare_and_swap(h, first, ei)) break;
    }
  }
}


void test_large_malloc(void) {
  size_t msize = 4*pagesize;
  int64_t fp = get_footprint();
  {
    void *x = large_malloc(msize);
    bassert(x);
    bassert(offset_in_chunk(x) == offset_of_first_object_in_large_chunk);

    void *y = large_malloc(msize);
    bassert(y);
    bassert(offset_in_chunk(y) == offset_of_first_object_in_large_chunk+msize);

    size_t fy = large_footprint(y);
    bassert(fy==msize);

    bassert(get_footprint() - fp == (int64_t)(2*msize));

    large_free(x);
    void *z = large_malloc(msize);
    bassert(z==x);

    large_free(z);
    large_free(y);
  }

  bassert(get_footprint() - fp == 0);
  {
    void *x = large_malloc(2*msize);
    bassert(x);
    bassert(offset_in_chunk(x) == offset_of_first_object_in_large_chunk+0*msize);

    bassert(get_footprint() - fp == (int64_t)(2*msize));

    void *y = large_malloc(2*msize);
    bassert(y);
    bassert(offset_in_chunk(y) == offset_of_first_object_in_large_chunk+2*msize);

    bassert(get_footprint() - fp == (int64_t)(4*msize));

    large_free(x);
    void *z = large_malloc(2*msize);
    bassert(z==x);

    large_free(z);
    large_free(y);
  }
  bassert(get_footprint() - fp == 0);
  {
    void *x = large_malloc(largest_large);
    bassert(x);
    bassert(offset_in_chunk(x) == offset_of_first_object_in_large_chunk + 0*largest_large);

    void *y = large_malloc(largest_large);
    bassert(y);
    bassert(offset_in_chunk(y) == offset_of_first_object_in_large_chunk + 1*largest_large);

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
    bassert(offset_in_chunk(x) == offset_of_first_object_in_large_chunk + 0*msize);

    bassert(large_footprint(x) == s);

    bassert(get_footprint() - fp == static_cast<int64_t>(s));

    void *y = large_malloc(s);
    bassert(y);
    bassert(offset_in_chunk(y) == offset_of_first_object_in_large_chunk + bin_2_size(size_2_bin(s)));

    bassert(large_footprint(y) == s);

    large_free(x);
    large_free(y);
  }
  bassert(get_footprint() - fp == 0);
}

#ifdef ENABLE_LOG_CHECKING
static const int log_count_limit = 10000000;
static int log_count=0;
static struct logentry {
  char command;
  const void* ptr;
} log[log_count_limit];

static void log_command(char command, const void *ptr) {
  int i = __sync_fetch_and_add(&log_count, 1);
  if (i < log_count_limit) {
    log[i].command = command;
    log[i].ptr     = ptr;
  } if (i == log_count_limit) {
    printf("Log overflowed, I dealt with that by truncating the log\n");
  }
}

void check_log_large() {
  printf("llog\n");
  for (int i = 0; i < log_count; i++) {
    printf("%c %p\n", log[i].command, log[i].ptr);
  }
}
#endif
