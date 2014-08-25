#include <assert.h>
#include <sys/mman.h>

#ifdef TESTING
#include <stdio.h>
#endif

#include "generated_constants.h"
#include "makehugepage.h"

// For each binned size, we need to maintain arrays of lists of pages.  The length of the array is the number of objects that fit into a page.
//  The first list is pages that have one empty slot.  The second list is pages with two empty slots.  The third list is pages with
//  three empty slots.
// The way we link pages together is that we use the empty slot as the next pointer, so we point at the empty slot.

void* huge_malloc(size_t size) {
  size_t n_chunks_base = ceil(size, chunksize);
  size_t n_chunks = hyperceil(n_chunks_base);
  void *c = mmap_chunk_aligned_block(n_chunks);
  size_t n_pages  = ceil(size, pagesize);
  size_t usable_size = n_pages * pagesize;
  size_t n_to_demap = n_chunks*chunksize - usable_size;
  binnumber_t bin;
  if (n_to_demap < chunksize/8) {
    // The unused part at the end is insubstantial, so just treat it as a malloc of a full chunk.
    // The whole region is be eligible for huge pages.
    madvise(c, n_chunks*chunksize, MADV_HUGEPAGE); // ignore any error code.  In future skip this call if we always get an error?  Also if we are in madvise=always we shouldn't bother.
    bin = size_2_bin(n_chunks*chunksize);
  } else {
    // The last chunk is not fully used.
    // Make all but the last chunk use huge pages, and the last chunk not.
    if (n_chunks>0) {
      madvise(c, (n_chunks-1)*chunksize, MADV_HUGEPAGE);
    }
    madvise((char *)c + (n_chunks-1)*chunksize, (n_pages*pagesize)%chunksize, MADV_NOHUGEPAGE);
    bin = size_2_bin(n_pages*pagesize);
  }
  uint64_t chunknum = chunk_number_of_address(c);
  chunk_infos[chunknum].bin_number = bin;
  return c;
}

#ifdef TESTING
void test_huge_malloc(void) {
  const bool print = false;

  void *a = huge_malloc(largest_large + 1);
  assert((uint64_t)a % chunksize==0);
  uint64_t a_n = chunk_number_of_address(a);
  if (print) printf("a=%p c=0x%lx\n", a, a_n);
  assert(chunk_infos[a_n].bin_number == first_huge_bin_number);
  *(char*)a = 1;

  void *b = huge_malloc(largest_large + 2);
  assert((uint64_t)b % chunksize==0);
  uint64_t b_n = chunk_number_of_address(b);
  if (print) printf("b=%p c=0x%lx diff=%ld\n", b, b_n, (char*)a-(char*)b);
  assert(a_n - b_n == 1);
  assert(chunk_infos[b_n].bin_number == first_huge_bin_number);

  void *c = huge_malloc(2*chunksize);
  assert((uint64_t)c % chunksize==0);
  uint64_t c_n = chunk_number_of_address(c);
  if (print) printf("c=%p diff=%ld bin = %u\n", c, (char*)b-(char*)c, chunk_infos[c_n].bin_number);
  assert(b_n - c_n == 2);
  assert(chunk_infos[c_n].bin_number == first_huge_bin_number -1 + ceil(2*chunksize - largest_large, pagesize));

  void *d = huge_malloc(2*chunksize);
  assert((uint64_t)d % chunksize==0);
  uint64_t d_n = chunk_number_of_address(d);
  if (print) printf("d=%p\n", d);
  assert(c_n - d_n == 2);
  assert(chunk_infos[c_n].bin_number == first_huge_bin_number -1 + ceil(2*chunksize - largest_large, pagesize));

  {
    uint64_t m1_n = chunk_number_of_address((void*)-1ul);
    if (print) printf("-1 ==> 0x%lx (1<<27)-1=%lx\n", m1_n, (1ul<<26)-1);
    assert(m1_n == (1ul<<27)-1);
    if (print) printf("-1 ==> 0x%lx\n", m1_n);
  }

  {
    uint64_t zero_n = chunk_number_of_address((void*)0);
    if (print) printf("0 ==> 0x%lx (1<<27)-1=%lx\n", zero_n, (1ul<<26)-1);
    assert(zero_n == 0);
    if (print) printf("-1 ==> 0x%lx\n", zero_n);
  }

}
#endif
