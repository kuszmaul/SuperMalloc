#ifdef TESTING
#include <stdio.h>
#endif

#include "atomically.h"
#include "malloc_internal.h"
#include "bassert.h"
#include "generated_constants.h"

struct large_object_list_cell {
  union {
    large_object_list_cell *next;
    uint32_t footprint;
  };
};

const binnumber_t n_large_classes = first_huge_bin_number - first_large_bin_number;
static large_object_list_cell* free_large_objects[n_large_classes]; // For each large size, a list (threaded through the chunk headers) of all the free objects of that size.
// Later we'll be a little careful about demapping those large objects (and we'll need to remember which are which, but we may also want thread-specific parts).  For now, just demap them all.

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
  bassert(b >= first_large_bin_number && b < first_huge_bin_number);

again:
  // This needs to be done atomically (along the successful branch)
  large_object_list_cell *h = free_large_objects[b];
  if (0) printf("h==%p\n", h);
  if (h != NULL) {
    free_large_objects[b] = h->next;
    // that was the atomic part.
    h->footprint = pagesize*ceil(size, pagesize);
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
    bassert(chunk!=0);
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
    entry[objects_per_chunk-1].next = free_large_objects[b];
    free_large_objects[b] = &entry[0];
    
    if (0) printf("Got object\n");

    goto again;
  }
}

size_t large_footprint(void *p) {
  if (0) printf("large_footprint(%p):\n", p);
  binnumber_t bin = chunk_infos[address_2_chunknumber(p)].bin_number;
  bassert(first_large_bin_number <= bin && bin < first_huge_bin_number);
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


void test_large_malloc(void) {
  {
    void *x = large_malloc(pagesize);
    bassert(x!=0);
    bassert((uint64_t)x % chunksize == 2*pagesize);

    void *y = large_malloc(pagesize);
    bassert(y!=0);
    bassert((uint64_t)y % chunksize == 3*pagesize);

    size_t fy = large_footprint(y);
    bassert(fy==pagesize);

    // need to free those
  }
  {
    void *x = large_malloc(2*pagesize);
    bassert(x!=0);
    bassert((uint64_t)x % chunksize == (2+0)*pagesize);

    void *y = large_malloc(2*pagesize);
    bassert(y!=0);
    bassert((uint64_t)y % chunksize == (2+2)*pagesize);
  }
  {
    void *x = large_malloc(largest_large);
    bassert(x!=0);
    bassert((uint64_t)x % chunksize == (2+0)*pagesize);

    void *y = large_malloc(largest_large);
    bassert(y!=0);
    bassert((uint64_t)y % chunksize == 2*pagesize + largest_large);
  }

  {
    size_t s = 100*4096;
    if (0) printf("s=%ld\n", s);

    void *x = large_malloc(s);
    bassert(x!=0);
    bassert((uint64_t)x % chunksize == (2+0)*pagesize);

    bassert(large_footprint(x) == s);

    void *y = large_malloc(s);
    bassert(y!=0);
    bassert((uint64_t)y % chunksize == 2*pagesize + bin_2_size(size_2_bin(s)));

    bassert(large_footprint(y) == s);
  }

}
