#include <sys/mman.h>
#include <algorithm>

#ifdef TESTING
#include <stdio.h>
#endif

#include "atomically.h"
#include "bassert.h"
#include "generated_constants.h"
#include "malloc_internal.h"

static lock_t huge_lock = LOCK_INITIALIZER;

// free_chunks[0] is a list of 1-chunk objects (which are, by definition chunk-aligned)
// free_chunks[1] is a list of 2-chunk objects which are also 2-chunk aligned (that is 4MiB-aligned).
// free_chunks[2] is a list of 4-chunk objects that are 4-chunk aligned.
// terminated by 0.
chunknumber_t free_chunks[log_max_chunknumber];

static void pre_get_from_free_chunks(int f) {
  int r = free_chunks[f];
  if (r==0) return;
  prefetch_write(&free_chunks[f]);
  prefetch_read(&chunk_infos[r]);
}
static void* do_get_from_free_chunks(int f) {
  chunknumber_t r = free_chunks[f];
  if (r==0) return NULL;
  free_chunks[f] = chunk_infos[r].next;
  return reinterpret_cast<void*>(static_cast<uint64_t>(r)*chunksize);
}

static void *get_cached_power_of_two_chunks(int list_number) {
  if (atomic_load(&free_chunks[list_number]) == 0) return NULL; // there are none.
  return atomically(&huge_lock, "huge:add_to_free_chunks", pre_get_from_free_chunks, do_get_from_free_chunks, list_number);
}

static void put_cached_power_of_two_chunks(chunknumber_t cn, int list_number) {
  // Do this atomically.  This one is simple enough to be done with a compare and swap.
  if (0) {
    chunk_infos[cn].next = free_chunks[list_number];
    free_chunks[list_number] = cn;
  } else {
    while (1) {
      chunknumber_t hd = atomic_load(&free_chunks[list_number]);
      chunk_infos[cn].next = hd;
      if (__sync_bool_compare_and_swap(&free_chunks[list_number], hd, cn)) break;
    }
  }
}

// static void* align_pointer_up_huge(void *p, chunknumber_t n_chunks_alignment) {
//   uint64_t ru = reinterpret_cast<uint64_t>(p);
//   uint64_t alignment = n_chunks_alignment * chunksize;
//   uint64_t ra = (ru + alignment -1) & ~(alignment-1);
//   return reinterpret_cast<void*>(ra);
// }

static void* get_power_of_two_n_chunks(chunknumber_t n_chunks)
// Effect: Allocate n_chunks of chunks.
// Requires: n_chunks is power of two.
{
  {
    void *r = get_cached_power_of_two_chunks(lg_of_power_of_two(n_chunks));
    if (r) return r;
  }
  void *p = mmap_chunk_aligned_block(2*n_chunks); 
  if (p == NULL) return NULL;
  chunknumber_t c = address_2_chunknumber(p);
  chunknumber_t end = c+2*n_chunks;
  void *result = NULL;
  while (c < end) {
    if ((c & (n_chunks-1)) == 0) {
      // c is aligned well enough
      result = reinterpret_cast<void*>(c*chunksize);
      c += n_chunks;
      break;
    } else {
      int bit = __builtin_ffs(c)-1;
      // make sure the bit we add doesn't overflow
      while (c + (1<<bit) > end) bit--;
      put_cached_power_of_two_chunks(c, bit);
      c += (1<<bit);
    }
  }
  // the pieces in the tail of c must be put in the right place.
  while (c < end) {
    int bit = __builtin_ffs(c)-1;
    while (c + (1<<bit) > end) bit--;
    put_cached_power_of_two_chunks(c, bit);
    c += (1<<bit);
  }
  bassert(result);
  return result;
}

void* huge_malloc(size_t size) {
  // allocates something out of the hyperceil(size) bin, which is also hyperceil(size)-aligned.
  chunknumber_t n_chunks = std::max(1ul, hyperceil(size)/chunksize); // at least one chunk always
  void *c = get_power_of_two_n_chunks(n_chunks);
  if (c == NULL) return NULL;
  madvise(c, n_chunks*chunksize, MADV_DONTNEED);
  size_t n_whole_chunks = size/chunksize;
  size_t n_bytes_at_end = size - n_whole_chunks*chunksize;
  if (n_bytes_at_end==0 ||
      (chunksize-n_bytes_at_end < chunksize/8)) {
    // The unused part at the end is either empty, or it's pretty big, so we'll just map it all as huge pages.
    madvise(c, n_chunks*chunksize, MADV_HUGEPAGE); // ignore any error code.  In future skip this call if we always get an error?  Also if we are in madvise=always we shouldn't bother.
  } else {
    // n_bytes_at_end != 0 and 
    // The unused part is smallish, so we'll use no-huge pages for it.
    if (n_whole_chunks>0) {
      madvise(c, n_whole_chunks*chunksize, MADV_HUGEPAGE);
    }
    madvise(reinterpret_cast<char*>(c) + n_whole_chunks*chunksize,
	    n_bytes_at_end,
	    MADV_NOHUGEPAGE);
  }
  chunknumber_t chunknum = address_2_chunknumber(c);
  binnumber_t bin        = size_2_bin(n_chunks*chunksize);
  bin_and_size_t b_and_s = bin_and_size_to_bin_and_size(bin, size);
  bassert(b_and_s != 0);
  chunk_infos[chunknum].bin_and_size = b_and_s;
  return c;
}

void huge_free(void *m) {
  // huge_free() is required to tolerate m being any pointer into the chunk returned by huge_malloc.
  // However this code cannot really tolerate i.
  m = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(m) & ~4095);
  bassert((reinterpret_cast<uint64_t>(m) & (chunksize-1)) == 0);
  chunknumber_t  cn  = address_2_chunknumber(m);
  bassert(cn);
  bin_and_size_t bnt = chunk_infos[cn].bin_and_size;
  bassert(bnt != 0);
  binnumber_t   bin  = bin_from_bin_and_size(bnt);
  uint64_t      siz  = bin_2_size(bin);
  chunknumber_t csiz = ceil(siz, chunksize);
  uint64_t     hceil = hyperceil(csiz);
  uint32_t      hlog = lg_of_power_of_two(hceil);
  bassert(hlog < log_max_chunknumber);
  {
    int r = madvise(m, siz, MADV_DONTNEED);
    bassert(r==0);  // Should we really check this?
  }
  put_cached_power_of_two_chunks(cn, hlog);
}

#ifdef TESTING
void test_huge_malloc(void) {
  const bool print = false;

  // Sometimes mmap works its way down (e.g., under linux 3.15.8).  Sometimes it works its way up (e.g., under valgrind under linux 3.15.8)
  // So the tests below have to look at the absolute difference instead of the relative difference.
  void *temp = huge_malloc(chunksize); // reset the strangeness that may have happened when testing the chunk allocator.
  if (print) printf("temp=%p\n", temp);

  void *a = huge_malloc(largest_large + 1);
  bassert(reinterpret_cast<uint64_t>(a) % chunksize==0);
  chunknumber_t a_n = address_2_chunknumber(a);
  if (print) printf("a=%p a_n=0x%x\n", a, a_n);
  bassert(bin_from_bin_and_size(chunk_infos[a_n].bin_and_size) >= first_huge_bin_number);
  *(char*)a = 1;

  void *b = huge_malloc(largest_large + 2);
  bassert(offset_in_chunk(b) == 0);
  chunknumber_t b_n = address_2_chunknumber(b);
  if (print) printf("b=%p diff=0x%lx a_n-b_n=%d\n", b, (char*)a-(char*)b, (int)a_n-(int)b_n);
  bassert(bin_from_bin_and_size(chunk_infos[b_n].bin_and_size) == first_huge_bin_number);

  void *c = huge_malloc(2*chunksize);
  bassert(offset_in_chunk(c) == 0);
  chunknumber_t c_n = address_2_chunknumber(c);
  if (print) printf("c=%p diff=0x%lx bin = %u,%u b_n=%d c_n=%d\n",
		    c, (char*)b-(char*)c,
		    bin_from_bin_and_size(chunk_infos[c_n].bin_and_size),
		    chunk_infos[c_n].bin_and_size>>7,
		    b_n, c_n);
  bassert(bin_from_bin_and_size(chunk_infos[c_n].bin_and_size) == first_huge_bin_number +1);

  void *d = huge_malloc(2*chunksize);
  bassert(reinterpret_cast<uint64_t>(d) % chunksize==0);
  chunknumber_t d_n = address_2_chunknumber(d);
  if (print) printf("d=%p c_n=%d d_n=%d diff=%d abs=%d\n", d, c_n, d_n, c_n-d_n, (int)std::abs((int)c_n-(int)d_n));
  bassert(bin_from_bin_and_size(chunk_infos[c_n].bin_and_size) == first_huge_bin_number +1);

  // Now make sure that a, b, c, d are allocated with no overlaps.
  bassert(abs(a_n-b_n)>=1);  // a and b must be separated by 1
  bassert(abs(a_n-c_n)>=2);  // a and c must be separated by 2
  bassert(abs(a_n-d_n)>=2);  // a and d must be separated by 2
  bassert(abs(b_n-c_n)>=2);  // b and c must be separated by 2
  bassert(abs(b_n-d_n)>=2);  // a and d must be separated by 2
  bassert(abs(c_n-d_n)>=2);  // c and d must be separated by 2

  {
    chunknumber_t m1_n = address_2_chunknumber(reinterpret_cast<void*>(-1ul));
    if (print) printf("-1 ==> 0x%x (1<<27)-1=%lx\n", m1_n, (1ul<<26)-1);
    bassert(m1_n == (1ul<<27)-1);
    if (print) printf("-1 ==> 0x%x\n", m1_n);
  }

  {
    chunknumber_t zero_n = address_2_chunknumber(reinterpret_cast<void*>(0));
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
