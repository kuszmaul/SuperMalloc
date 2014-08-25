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

void* get_power_of_two_n_chunks(size_t n_chunks)
// Effect: Allocate n_chunks of chunks.
// Requires: n_chunks is power of two.
{
  int f = lg_of_power_of_two(n_chunks);
  if (free_chunks[f]==0) {
    return mmap_chunk_aligned_block(n_chunks);
  } else {
    // Do this atomically.
    chunknumber_t r = free_chunks[f];
    free_chunks[f] = chunk_infos[r].next;
    return (void*)((uint64_t)r*chunksize);
  }
}

void* huge_malloc(size_t size) {
  size_t n_chunks_base = ceil(size, chunksize);
  size_t n_chunks = hyperceil(n_chunks_base);
  void *c = get_power_of_two_n_chunks(n_chunks);
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
  chunknumber_t chunknum = address_2_chunknumber(c);
  chunk_infos[chunknum].bin_number = bin;
  return c;
}

void huge_free(void *m) {
  chunknumber_t cn = address_2_chunknumber(m);
  assert(cn!=0);
  binnumber_t bin = chunk_infos[cn].bin_number;
  uint64_t    siz = bin_2_size(bin);
  uint64_t    hceil = hyperceil(siz);
  uint32_t    hlog  = lg_of_power_of_two(hceil);
  assert(hlog < log_max_chunknumber);
  madvise(m, siz, MADV_DONTNEED);
  // Do this atomically.
  chunk_infos[cn].next = free_chunks[hlog];
  free_chunks[hlog] = cn;
}

#ifdef TESTING
void test_huge_malloc(void) {
  const bool print = false;

  void *a = huge_malloc(largest_large + 1);
  assert((uint64_t)a % chunksize==0);
  chunknumber_t a_n = address_2_chunknumber(a);
  if (print) printf("a=%p c=0x%x\n", a, a_n);
  assert(chunk_infos[a_n].bin_number == first_huge_bin_number);
  *(char*)a = 1;

  void *b = huge_malloc(largest_large + 2);
  assert((uint64_t)b % chunksize==0);
  chunknumber_t b_n = address_2_chunknumber(b);
  if (print) printf("b=%p c=0x%x diff=%ld\n", b, b_n, (char*)a-(char*)b);
  assert(a_n - b_n == 1);
  assert(chunk_infos[b_n].bin_number == first_huge_bin_number);

  void *c = huge_malloc(2*chunksize);
  assert((uint64_t)c % chunksize==0);
  chunknumber_t c_n = address_2_chunknumber(c);
  if (print) printf("c=%p diff=%ld bin = %u\n", c, (char*)b-(char*)c, chunk_infos[c_n].bin_number);
  assert(b_n - c_n == 2);
  assert(chunk_infos[c_n].bin_number == first_huge_bin_number -1 + ceil(2*chunksize - largest_large, pagesize));

  void *d = huge_malloc(2*chunksize);
  assert((uint64_t)d % chunksize==0);
  chunknumber_t d_n = address_2_chunknumber(d);
  if (print) printf("d=%p\n", d);
  assert(c_n - d_n == 2);
  assert(chunk_infos[c_n].bin_number == first_huge_bin_number -1 + ceil(2*chunksize - largest_large, pagesize));

  {
    chunknumber_t m1_n = address_2_chunknumber((void*)-1ul);
    if (print) printf("-1 ==> 0x%x (1<<27)-1=%lx\n", m1_n, (1ul<<26)-1);
    assert(m1_n == (1ul<<27)-1);
    if (print) printf("-1 ==> 0x%x\n", m1_n);
  }

  {
    chunknumber_t zero_n = address_2_chunknumber((void*)0);
    if (print) printf("0 ==> 0x%x (1<<27)-1=%lx\n", zero_n, (1ul<<26)-1);
    assert(zero_n == 0);
    if (print) printf("-1 ==> 0x%x\n", zero_n);
  }

}
#endif
