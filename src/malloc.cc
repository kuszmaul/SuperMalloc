#include "supermalloc.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#ifdef TESTING
#include <cstdio>
#endif


#include "atomically.h"
#include "bassert.h"
#include "cpucores.h"
#include "generated_constants.h"

#ifdef TESTING
extern "C" void test_size_2_bin(void) {
    for (size_t i=8; i<=largest_large; i++) {
        binnumber_t g = size_2_bin(i);
        bassert(g<first_huge_bin_number);
        bassert(i <= static_bin_info[g].object_size);
        if (g>0) bassert(i > static_bin_info[g-1].object_size);
        else bassert(AND(g==0, i==8));
	size_t s = bin_2_size(g);
	bassert(s>=i);
	bassert(size_2_bin(s) == g);
    }
    for (size_t i = largest_large+1; i <= chunksize; i++) {
      bassert(size_2_bin(i) == first_huge_bin_number);
    }
    bassert(bin_2_size(first_huge_bin_number-1) < chunksize);
    bassert(bin_2_size(first_huge_bin_number) == chunksize);
    bassert(bin_2_size(first_huge_bin_number+1) == chunksize*2);
    bassert(bin_2_size(first_huge_bin_number+2) == chunksize*4);
    for (int k = 0; k < 1000; k++) {
      size_t s = chunksize * 10 + pagesize * k;
      binnumber_t b = size_2_bin(s);
      bassert(size_2_bin(bin_2_size(b))==b);
      bassert(bin_2_size(size_2_bin(s))==hyperceil(s));
    }

    // Verify that all the bins that are 256 or larger are multiples of a cache line.
    for (binnumber_t i = 0; i <= first_huge_bin_number; i++) {
      size_t os = static_bin_info[i].object_size;
      bassert(OR(os < 256, os%64 == 0));
    }
}
#endif

static unsigned int initialize_lock=0;
struct chunk_info *chunk_infos;

uint32_t n_cores;

#ifndef USE_PTHREAD_MUTEXES
#ifdef DO_FAILED_COUNTS
atomic_stats_s atomic_stats;

lock_t failed_counts_mutex = LOCK_INITIALIZER;
int    failed_counts_n = 0;
struct failed_counts_s failed_counts[max_failed_counts];

int compare_failed_counts(const void *a_v, const void *b_v) {
  const failed_counts_s *a = reinterpret_cast<const failed_counts_s*>(a_v);
  const failed_counts_s *b = reinterpret_cast<const failed_counts_s*>(b_v);
  int r = strcmp(a->name, b->name);
  if (r!=0) return r;
  if (a->code < b->code) return -1;
  if (a->code > b->code) return +1;
  return 0;
}
#endif
static void print_atomic_stats() {
#ifdef DO_FAILED_COUNTS
  fprintf(stderr, "Critical sections: %ld, locked %ld\n", atomic_stats.atomic_count, atomic_stats.locked_count);
  qsort(failed_counts, failed_counts_n, sizeof(failed_counts[0]), compare_failed_counts);
  for (int i = 0; i < failed_counts_n; i++) {
    fprintf(stderr, " %38s: 0x%08x %5ld\n", failed_counts[i].name, failed_counts[i].code, failed_counts[i].count);
  }
#endif
}
#else
static void print_atomic_stats() {
}
#endif

#ifdef ENABLE_STATS
static void print_stats() {
  print_cache_stats();
  print_bin_stats();
}
#endif

#ifdef ENABLE_LOG_CHECKING
static void check_log() {
  check_log_large();
}
#endif

bool use_transactions = true;
bool do_predo = true;
bool use_threadcache = true;

#ifndef TESTING
static
#else
extern "C"
#endif
void initialize_malloc() {
  atexit(print_atomic_stats);
#ifdef ENABLE_STATS
  atexit(print_stats);
#endif
#ifdef ENABLE_LOG_CHECKING
  atexit(check_log);
#endif

  const size_t n_elts = 1u<<27;
  const size_t alloc_size = n_elts * sizeof(chunk_info);
  const size_t n_chunks   = ceil(alloc_size, chunksize);
  chunk_infos = (chunk_info*)mmap_chunk_aligned_block(n_chunks);
  bassert(chunk_infos);

  n_cores = cpucores();

  {
    char *v = getenv("SUPERMALLOC_TRANSACTIONS");
    if (v) {
      if (strcmp(v, "0")==0) {
	use_transactions  = false;
      } else if (strcmp(v, "1")==0) {
	use_transactions = true;
      }
    }
  }
  {
    char *v = getenv("SUPERMALLOC_PREDO");
    if (v) {
      if (strcmp(v, "0")==0) {
	do_predo  = false;
      } else if (strcmp(v, "1")==0) {
	do_predo = true;
      }
    }
  }
  {
    char *v = getenv("SUPERMALLOC_THREADCACHE");
    if (v) {
      if (strcmp(v, "0")==0) {
	use_threadcache = false;
      } else if (strcmp(v, "1")==0) {
	use_threadcache = true;
      }
    }
  }
}

void maybe_initialize_malloc(void) {
  // This should be protected by a lock.
  if (atomic_load(&chunk_infos)) return;
  while (__sync_lock_test_and_set(&initialize_lock, 1)) {
    _mm_pause();
  }
  if (!chunk_infos) initialize_malloc();
  __sync_lock_release(&initialize_lock);
}

#ifdef TESTING
static uint64_t slow_hyperceil(uint64_t a) {
  uint64_t r = 1;
  a--;
  while (a > 0) {
    a /= 2;
    r *= 2;
  }
  return r;
}

static void test_hyperceil_v(uint64_t a, uint64_t expected) {
  bassert(hyperceil(a)==slow_hyperceil(a));
  bassert(hyperceil(a)==expected);
}

void test_hyperceil(void) {
  test_hyperceil_v(1, 1);
  test_hyperceil_v(2, 2);
  test_hyperceil_v(3, 4);
  test_hyperceil_v(4, 4);
  test_hyperceil_v(5, 8);
  for (int i = 3; i < 27; i++) {
    test_hyperceil_v((1u<<i)+0,   (1u<<i));
    test_hyperceil_v((1u<<i)-1,   (1u<<i));
    test_hyperceil_v((1u<<i)+1, 2*(1u<<i));
  }
}
#endif

static uint64_t max_allocatable_size = (chunksize << 27)-1;

// Three kinds of mallocs:
//   BIG, used for large allocations.  These are 2MB-aligned chunks.  We use BIG for anything bigger than a quarter of a chunk.
//   SMALL fit within a chunk.  Everything within a single chunk is the same size.
// The sizes are the powers of two (1<<X) as well as (1<<X)*1.25 and (1<<X)*1.5 and (1<<X)*1.75
extern "C" void* malloc(size_t size) {
  maybe_initialize_malloc();
  if (size >= max_allocatable_size) {
    errno = ENOMEM;
    return NULL;
  }
  if (size <= largest_large) {
    binnumber_t bin = size_2_bin(size);
    size_t siz = bin_2_size(bin);
    if (!is_power_of_two(siz/cacheline_size))
      return cached_malloc(bin);
    if (bin+1 < first_huge_bin_number)
      return cached_malloc(bin+1);
    // Else fall through,
  }
  char *result = reinterpret_cast<char*>(huge_malloc(size+pagesize-cacheline_size));
  int   add_misalignment = cacheline_size * (prandnum()%(pagesize/cacheline_size));
  return result+add_misalignment;  
}

extern "C" void free(void *p) {
  maybe_initialize_malloc();
  if (p == NULL) return;
  chunknumber_t cn = address_2_chunknumber(p);
  bin_and_size_t bnt = chunk_infos[cn].bin_and_size;
  binnumber_t bin = bin_from_bin_and_size(bnt);
  bassert(!(offset_in_chunk(p) == 0 && bin==0)); // we cannot have a bin 0 item that is chunk-aligned
  if (bin < first_huge_bin_number) {
    cached_free(p, bin);
  } else {
    // If the bin is a sentinal, then that means it's huge_free()'s problem to figure it out.
    // Huge free will scan backwards to find the proper size (which is needed only for
    // aligned allocations).
    huge_free(p);
  }
}

extern "C" void* realloc(void *p, size_t size) {
  if (size >= max_allocatable_size) {
    errno = ENOMEM;
    return NULL;
  }
  if (p == NULL) return malloc(size);
  size_t oldsize = malloc_usable_size(p);
  if (oldsize < size) {
    void *result = malloc(size);
    if (!result) return NULL; // without disrupting the contents of p.
    for (size_t i = 0; i < oldsize; i++) {
      ((char*)result)[i] = ((char*)p)[i];
    }
    free(p);
    return result;
  }
  if (oldsize > 16 && size < oldsize/2) {
    void *result = malloc(size);
    if (!result) return NULL; // without disrupting the contents of p.
    for (size_t i = 0; i < size; i++) {
      ((char*)result)[i] = ((char*)p)[i];
    }
    return result;
  }
  return p;
}

#ifdef TESTING
void test_realloc(void) {
  char *a = (char*)malloc(128);
  for (int i = 0; i < 128; i++) a[i]='a';
  char *b = (char*)realloc(a, 1+malloc_usable_size(a));
  bassert(a != b);
  for (int i = 0; i < 128; i++) bassert(b[i]=='a');
  bassert(malloc_usable_size(b) >= 129);
  char *c = (char*)realloc(b, 32);
  bassert(c != b);
  for (int i = 0; i < 32; i++) bassert(c[i]=='a');
  char *d = (char*)realloc(c, 31);
  bassert(c==d);
  free(d);
}
#endif

extern "C" void* calloc(size_t number, size_t size) {
  void *result = malloc(number*size);

  void *base = object_base(result);
  size_t usable_from_base = malloc_usable_size(base);
  uint64_t oip = offset_in_page(base);

  if (oip > 0) {
    // if the base object is not page aligned, then it's a small object.  Just zero it.
    memset(result, 0, number*size);
  } else if (usable_from_base % pagesize != 0) {
    // If the base object is page aligned, and the usable amount isn't page aligned, it's still pretty small, so just zero it.
    bassert(usable_from_base < chunksize);
    memset(result, 0, number*size);
  } else {
    // everything is page aligned.
    madvise(base, usable_from_base, MADV_DONTNEED);
  }
  return result;
}

static void* align_pointer_up(void *p, uint64_t alignment, uint64_t size, uint64_t alloced_size) {
  uint64_t ru = reinterpret_cast<uint64_t>(p);
  uint64_t ra = (ru + alignment -1) & ~(alignment-1);
  bassert((ra & (alignment-1)) == 0);
  bassert(ra + size <= ru + alloced_size);
  return reinterpret_cast<void*>(ra);
}

static void* aligned_malloc_internal(size_t alignment, size_t size) {
  // requires alignment is a power of two.
  maybe_initialize_malloc();
  binnumber_t bin = size_2_bin(size);
  while (bin < first_huge_bin_number) {
    uint64_t bs = bin_2_size(bin);
    if (0 == (bs & (alignment -1))) {
      // this bin produced blocks that are aligned with alignment
      return cached_malloc(bin);
    }
    if (bs+1 >= alignment+size) {
      // this bin produces big enough blocks to force alignment by taking a subpiece.
      return align_pointer_up(cached_malloc(bin), alignment, size, bs);
    }
    bin++;
  }
  // We fell out the bottom.  We'll use a huge block.
  if (alignment <= chunksize) {
    // huge blocks are naturally aligned properly.
    return huge_malloc(size); // huge blocks are always naturally aligned.
  } else {
    // huge blocks are naturally powers of two, but they aren't always aligned.  Allocate something big enough to align it.
    // huge_malloc sets all the intermediate spots to bin -1 to indicate that it's not really the beginning.
    void *r = huge_malloc(std::max(alignment, size)); // this will be aligned.  The bookkeeping will be messed up if alignment>size, however.
    bassert((reinterpret_cast<uint64_t>(r) & (alignment-1)) == 0); // make sure it is aligned
    return r;
  }
}


extern "C" void* aligned_alloc(size_t alignment, size_t size) __THROW {
  if (size >= max_allocatable_size) {
    errno = ENOMEM;
    return NULL;
  }
  if (alignment & (alignment-1)) {
    // alignment must be a power of two
    errno = EINVAL;
    return NULL;
  }
  if ((size & (alignment-1)) != 0)  {
    // size must be an integral multiple of alignment, which is easy to test since alignment is a power of two.
    errno = EINVAL;
    return NULL;
  }
  return aligned_malloc_internal(alignment, size);
}

extern "C" int posix_memalign(void **ptr, size_t alignment, size_t size) {
  if (alignment & (alignment -1)) {
    // alignment must be a power of two.
    return EINVAL;
  }
  if (alignment < sizeof(void*)) {
    // alignment must be at least sizeof void*.
    return EINVAL;
  }
  if (size == 0) {
    *ptr = NULL;
    return 0;
  }
  *ptr = aligned_malloc_internal(alignment, size);
  return 0;
}

extern "C" size_t malloc_usable_size(const void *ptr) {
  chunknumber_t cn = address_2_chunknumber(ptr);
  binnumber_t bin = bin_from_bin_and_size(chunk_infos[cn].bin_and_size);
  const char *base = reinterpret_cast<const char*>(object_base(const_cast<void*>(ptr)));
  const char *ptr_c = reinterpret_cast<const char*>(ptr);
  ssize_t base_size = bin_2_size(bin);
  bassert(base <= ptr);
  bassert(base_size >= ptr_c-base);
  return base_size - (ptr_c-base);
}

#ifdef TESTING
static void test_malloc_usable_size_internal(size_t given_s) {
  char *a = reinterpret_cast<char*>(malloc(given_s));
  size_t as = malloc_usable_size(a);
  char *base = reinterpret_cast<char*>(object_base(a));
  binnumber_t b = size_2_bin(malloc_usable_size(base));
  bassert(malloc_usable_size(base) == bin_2_size(b));
  bassert(malloc_usable_size(base) + base == malloc_usable_size(a) + a);  
  if (b < first_huge_bin_number) {
    bassert(address_2_chunknumber(a) == address_2_chunknumber(a+as-1));
  } else {
    bassert(offset_in_chunk(base) == 0);
  }
  free(a);
}

void test_malloc_usable_size(void) {
  for (size_t i=8; i<4*chunksize; i*=2) {
    for (size_t o=0; o<8; o++) {
      test_malloc_usable_size_internal(i+o);
    }
  }
}
#endif

void* object_base(void *ptr) {
  // Requires: ptr is on the same chunk as the object base.
  chunknumber_t cn = address_2_chunknumber(ptr);
  binnumber_t bin = bin_from_bin_and_size(chunk_infos[cn].bin_and_size);
  if (bin >= first_huge_bin_number) {
    return address_2_chunkaddress(ptr);
  } else {
    uint64_t wasted_offset   = static_bin_info[bin].overhead_pages_per_chunk * pagesize;
    uint64_t useful_offset   = offset_in_chunk(ptr) - wasted_offset;
    uint32_t folio_number    = divide_offset_by_foliosize(useful_offset, bin);
    uint64_t folio_mul       = folio_number * static_bin_info[bin].folio_size;
    uint32_t offset_in_folio = useful_offset - folio_mul;
    uint64_t object_number   = divide_offset_by_objsize(offset_in_folio, bin);
    return reinterpret_cast<void*>(cn*chunksize + wasted_offset + folio_mul + object_number * static_bin_info[bin].object_size);
  }
}

#ifdef TESTING
void test_object_base() {
  void *p = malloc(8193);
  //printf("objbase p      =%p\n", p);
  //printf("        objbase=%p\n", object_base(p));
  bassert(offset_in_chunk(object_base(p)) >= 4096);
}
#endif

// The basic idea of allocation, is that that we allocate 2MiB chunks
// (which are 2MiB aligned), and everything within a 2MiB chunk is the
// same size.

// For small objects: we keep meta-information at the beginning of
// each page:
//
//  * a free list (this is a 16 bits since the most objects per page
//    is less than 512.)
//
//  * a count of the number of free slots (also 16 bits, since most
//    objects per page is 512)
//
//  * two full pointers (to other pages) which can be used to
//    implement a heap (to get the pages with the fewest free slots)
//
// Therefore the overhead is 20 bytes.  Let's round to 64 bytes so
// that objects will be cache aligned to the extent possible.  (This
// choice costs us a few lost objects per page for the small sizes
// (objects of size 40 or smaller) So the objects size out as shown in
// the table below. We waste a few hundred bytes for per page for the
// last few objects (5% of the page).  
//
// Given that we waste on average half the difference between
// successive objets, for 256 to 320 we are wasting another 32 bytes
// per object on average, which is another 32*12 = 384 bytes wasted on
// average.  So the wasted 236 isn't too bad, since the total is 7%
// for that size.
//
// For medium objects, we fit as many as we can per page, and make the
// objects be 64-byte aligned.  Almost no space is wasted on these
// objects which range from 9 per page to 2 per page.
//
// Large objects are allocated as a multiple of page sizes.  The worst
// case is if you allocate a 4097 byte object.  You get two pages, for
// nearly a factor of 2x wastage.  We have one bin for each power of
// two: thus we have a 4KiB bin, an 8KiB bin, a 16KiB bin and so
// forth.  If you want a 12KiB object, we allocate it in a 16KiB bin,
// and rely on the last 4KiB not actually using resident memory.  We
// allocate this way so that we don't have to remember how many pages
// are allocated for an object or manage the fragmentation of those
// pages.  This means that we end up with 9 bins of large objects, for
// a total of 40 bins.
//
// Huge  objects are allocated as a multiple of the chunk size.
//
// Here are the non-huge sizes:
//
// make objsizes && ./objsizes 
// pagesize = 4096 bytes
// overhead = 64 bytes per page
// small objects:
// bin   objsize  objs/page  wastage
//   0         8      504       0
//   1        10      403       2
//   2        12      336       0
//   3        14      288       0
//   4        16      252       0
//   5        20      201      12
//   6        24      168       0
//   7        28      144       0
//   8        32      126       0
//   9        40      100      32
//  10        48       84       0
//  11        56       72       0
//  12        64       63       0
//  13        80       50      32
//  14        96       42       0
//  15       112       36       0
//  16       128       31      64
//  17       160       25      32
//  18       192       21       0
//  19       224       18       0
//  20       256       15     192
//  21       320       12     192
//  22       384       10     192
//  23       448        9       0
// medium objects:
//  24       504        8       0
//  25       576        7       0
//  26       672        6       0
//  27       806        5       2
//  28      1008        4       0
//  29      1344        3       0
//  30      2016        2       0
// large objects (page allocated):
//  31      1<<12
//  32      1<<13
//  33      1<<14
//  34      1<<15
//  35      1<<16
//  36      1<<17
//  37      1<<18
//  38      1<<19
//  39      1<<20
//
// We maintain a table which is simply the object size for each chunk.
// This is just a big array indexed by chunk number.  The chunk number
// is gotten by taking the chunk address and shifting it right by 21
// (sign extending) and adding an offset so that the index ranges from
// 0 (inclusive) to 2^{27} (exclusive).  The table contains a bin
// number, except for huge objects where it contains the number of
// chunks.  This allows the table to be kept with a single 32-bit
// number, making the entire table 2^{29} bytes (512MiB).  Again we
// rely on the table no being all mapped into main memory, but it
// might make good sense for this table to use transparent huge pages,
// even at the beginning, since it probably means a single page table
// entry for this table.

bin_and_size_t bin_and_size_to_bin_and_size(binnumber_t bin, size_t size) {
  bassert(bin < 128);
  uint32_t n_pages = ceil(size,4096);
  if (n_pages  < (1<<24)) {
    return bin + (1<<7) + (n_pages<<8);
  } else {
    return bin +          (ceil(size,chunksize)<<8);
  }
}
