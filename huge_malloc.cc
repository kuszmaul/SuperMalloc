#include <sys/mman.h>

#ifdef TESTING
#include <stdio.h>
#include <algorithm>
#endif

#include "atomically.h"
#include "malloc_internal.h"
#include "bassert.h"
#include "generated_constants.h"

// For each binned size, we need to maintain arrays of lists of pages.  The length of the array is the number of objects that fit into a page.
//  The first list is pages that have one empty slot.  The second list is pages with two empty slots.  The third list is pages with
//  three empty slots.
// The way we link pages together is that we use the empty slot as the next pointer, so we point at the empty slot.

static unsigned int huge_lock = 0;
struct atfc {
  int f;
  void* result;
  chunknumber_t u;
};
static void pre_add_to_free_chunks(void *extra) {
  struct atfc *a = (struct atfc*)extra;
  int f = a->f;
  int r = free_chunks[f];
  prefetch_write(&free_chunks[f]);
  a->u = chunk_infos[r].next;
}
static void do_add_to_free_chunks(void *extra) {
  struct atfc *a = (struct atfc*)extra;
  int f = a->f;
  chunknumber_t r = free_chunks[f];
  free_chunks[f] = chunk_infos[r].next;
  a->result = (void*)((uint64_t)r*chunksize);
}

static void* get_power_of_two_n_chunks(chunknumber_t n_chunks)
// Effect: Allocate n_chunks of chunks.
// Requires: n_chunks is power of two.
{
  int f = lg_of_power_of_two(n_chunks);
  if (0) printf("Getting %d chunks. Tryin free_chunks[%d]\n", n_chunks, f);
  if (atomic_load(&free_chunks[f]) ==0) {
    return mmap_chunk_aligned_block(n_chunks);
  } else {
    // Do this atomically.
    atfc a = {f, NULL, 0};
    atomically(&huge_lock, pre_add_to_free_chunks, do_add_to_free_chunks, (void*)&a);
    return a.result;
  }
}

void* huge_malloc(size_t size) {
  chunknumber_t n_chunks_base = ceil(size, chunksize);
  chunknumber_t n_chunks = hyperceil(n_chunks_base);
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
    if (0) printf("malloc(%ld) got %d chunks\n", size, n_chunks);
    if (n_chunks>1) {
      if (0) printf(" madvise size=%ld HUGEPAGE\n", (n_chunks-1)*chunksize);
      madvise(c, (n_chunks-1)*chunksize, MADV_HUGEPAGE);
    }
    madvise((char *)c + (n_chunks-1)*chunksize, (n_pages*pagesize)%chunksize, MADV_NOHUGEPAGE);
    bin = size_2_bin(n_pages*pagesize);
  }
  chunknumber_t chunknum = address_2_chunknumber(c);
  chunk_infos[chunknum].bin_number = bin;
  if (0) printf(" malloced %p\n", c);
  return c;
}

void huge_free(void *m) {
  chunknumber_t  cn  = address_2_chunknumber(m);
  bassert(cn!=0);
  binnumber_t   bin  = chunk_infos[cn].bin_number;
  uint64_t      siz  = bin_2_size(bin);
  chunknumber_t csiz = siz/chunksize;
  uint64_t     hceil = hyperceil(csiz);
  uint32_t      hlog = lg_of_power_of_two(hceil);
  bassert(hlog < log_max_chunknumber);
  madvise(m, siz, MADV_DONTNEED);
  // Do this atomically.  This one is simple enough to be done with a compare and swap.
  
  if (0) {
      chunk_infos[cn].next = free_chunks[hlog];
      free_chunks[hlog] = cn;
  } else {
    while (1) {
      chunknumber_t hd = atomic_load(&free_chunks[hlog]);
      chunk_infos[cn].next = hd;
      if (__sync_bool_compare_and_swap(&free_chunks[hlog], hd, cn)) break;
    }
  }
}

#ifdef TESTING
void test_huge_malloc(void) {
  const bool print = false;

  // Sometimes mmap works its way down (e.g., under linux 3.15.8).  Sometimes it works its way up (e.g., under valgrind under linux 3.15.8)
  // So the tests below have to look at the absolute difference instead of the relative difference.

  void *a = huge_malloc(largest_large + 1);
  bassert((uint64_t)a % chunksize==0);
  chunknumber_t a_n = address_2_chunknumber(a);
  if (print) printf("a=%p c=0x%x\n", a, a_n);
  bassert(chunk_infos[a_n].bin_number == first_huge_bin_number);
  *(char*)a = 1;

  void *b = huge_malloc(largest_large + 2);
  bassert((uint64_t)b % chunksize==0);
  chunknumber_t b_n = address_2_chunknumber(b);
  if (print) printf("b=%p c=0x%x diff=%ld\n", b, b_n, (char*)a-(char*)b);
  bassert(abs((int)a_n - (int)b_n) == 1);
  bassert(chunk_infos[b_n].bin_number == first_huge_bin_number);

  void *c = huge_malloc(2*chunksize);
  bassert((uint64_t)c % chunksize==0);
  chunknumber_t c_n = address_2_chunknumber(c);
  if (print) printf("c=%p diff=%ld bin = %u b_n=%d c_n=%d\n", c, (char*)b-(char*)c, chunk_infos[c_n].bin_number, b_n, c_n);
  bassert((b_n - c_n == 2) || (c_n - b_n ==1));
  bassert(chunk_infos[c_n].bin_number == first_huge_bin_number -1 + ceil(2*chunksize - largest_large, pagesize));

  void *d = huge_malloc(2*chunksize);
  bassert((uint64_t)d % chunksize==0);
  chunknumber_t d_n = address_2_chunknumber(d);
  if (print) printf("d=%p c_n=%d d_n=%d diff=%d abs=%d\n", d, c_n, d_n, c_n-d_n, (int)std::abs((int)c_n-(int)d_n));
  bassert(std::abs((int)c_n - (int)d_n) == 2);
  bassert(chunk_infos[c_n].bin_number == first_huge_bin_number -1 + ceil(2*chunksize - largest_large, pagesize));

  {
    chunknumber_t m1_n = address_2_chunknumber((void*)-1ul);
    if (print) printf("-1 ==> 0x%x (1<<27)-1=%lx\n", m1_n, (1ul<<26)-1);
    bassert(m1_n == (1ul<<27)-1);
    if (print) printf("-1 ==> 0x%x\n", m1_n);
  }

  {
    chunknumber_t zero_n = address_2_chunknumber((void*)0);
    if (print) printf("0 ==> 0x%x (1<<27)-1=%lx\n", zero_n, (1ul<<26)-1);
    bassert(zero_n == 0);
    if (print) printf("-1 ==> 0x%x\n", zero_n);
  }

  huge_free(a);
  void *a_again = huge_malloc(largest_large + 1);
  if (print) printf("a=%p a_again=%p\n", a, a_again);
  bassert(a==a_again);

  huge_free(a_again);
  huge_free(b);
  void *b_again      = huge_malloc(largest_large + 2);
  void *a_againagain = huge_malloc(largest_large + 1);
  if (print) printf("a=%p b=%p a_again=%p b_again=%p\n", a, b, a_again, b_again);
  bassert(b==b_again);
  bassert(a==a_againagain);

  huge_free(d);
  void *d_again      = huge_malloc(2*chunksize);
  bassert(d==d_again);

  // Make sure the chunk cache works right when we ask for a different size.
  // Recall that the reason we do the bookkeeping separately after the chunks are
  // allocated is so that we can keep track of the footprint (the footprint is the
  // RSS if the user were to touch all the byte of all her malloc'd objects.)
  void *e            = huge_malloc(5*chunksize);
  huge_free(e);
  void *eagain       = huge_malloc(8*chunksize);
  bassert(e==eagain);
  huge_free(e);

  void *f            = huge_malloc(5*chunksize+1);
  bassert(f==e);
  huge_free(f);

  huge_free(a_againagain);
  void *g            = huge_malloc(chunksize-4096);
  bassert(g==a_againagain);
  

}
#endif
