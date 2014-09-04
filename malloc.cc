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
static void test_size_2_bin(void) {
    for (size_t i=8; i<=largest_large; i++) {
        binnumber_t g = size_2_bin(i);
        bassert(g<first_huge_bin_number);
        bassert(i <= static_bin_info[g].object_size);
        if (g>0) bassert(i > static_bin_info[g-1].object_size);
        else bassert(g==0 && i==8);
	size_t s = bin_2_size(g);
	bassert(s>=i);
	bassert(size_2_bin(s) == g);
    }
    bassert(size_2_bin(largest_large+1) == first_huge_bin_number);
    bassert(size_2_bin(largest_large+4096) == first_huge_bin_number);
    bassert(size_2_bin(largest_large+4096+1) == 1+first_huge_bin_number);
    bassert(bin_2_size(first_huge_bin_number) == largest_large+4096);
    bassert(bin_2_size(first_huge_bin_number+1) == largest_large+4096*2);
    bassert(bin_2_size(first_huge_bin_number+2) == largest_large+4096*3);
    for (int k = 0; k < 1000; k++) {
      size_t s = chunksize * 10 + pagesize * k;
      binnumber_t b = size_2_bin(s);
      bassert(size_2_bin(bin_2_size(b))==b);
      bassert(bin_2_size(size_2_bin(s))==s);
    }

    // Verify that all the bins that are 256 or larger are multiples of a cache line.
    for (binnumber_t i = 0; i <= first_huge_bin_number; i++) {
      size_t os = static_bin_info[i].object_size;
      bassert(os < 256 || os%64 == 0);
    }
}
#endif

static unsigned int initialize_lock=0;
struct chunk_info *chunk_infos;

uint32_t n_cores;

void initialize_malloc(void) {
  const size_t n_elts = 1u<<27;
  const size_t alloc_size = n_elts * sizeof(chunk_info);
  const size_t n_chunks   = ceil(alloc_size, chunksize);
  chunk_infos = (chunk_info*)mmap_chunk_aligned_block(n_chunks);
  bassert(chunk_infos);

  n_cores = cpucores();
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
  if (expected) {
    bassert(hyperceil(a)==expected);
  }
}

static void test_hyperceil(void) {
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

static __thread uint32_t cached_cpu, cached_cpu_count;
static uint32_t getcpu(void) {
  if ((cached_cpu_count++)%2048  ==0) { cached_cpu = sched_getcpu(); if (0) printf("cpu=%d\n", cached_cpu); }
  return cached_cpu;
}

struct linked_list {
  linked_list *next;
};

const int cpulimit = 128;
struct cached_objects {
  uint64_t bytecount;
  linked_list *head;
};
struct CacheForCpu {
  volatile unsigned int lock;
  uint64_t attempt_count, success_count;
  cached_objects c[first_large_bin_number] __attribute__((aligned(16))); // it's OK if the cached objects are on the same cacheline as the lock.
} __attribute__((aligned(64)));

CacheForCpu cache_for_cpu[cpulimit];

struct get_cached_s {
  struct cached_objects *c;
  uint64_t size;
  void *result;
} __attribute__((aligned(64)));
static void predo_get_cached(void *vv) {
  get_cached_s *s = (get_cached_s*)vv;
  linked_list *h = s->c->head;
  prefetch_write(&s->result);
  prefetch_write(&s->c->head);
  prefetch_write(&s->c->bytecount);
  if (h != NULL)  {
    prefetch_read(&h->next);
  }
}
static void do_get_cached(void *vv) {
  get_cached_s *s = (get_cached_s*)vv;
  linked_list *h = s->c->head;
  if (h == NULL) {
    s->result = NULL;
  } else {
    s->result = h;
    s->c->head = h->next;
    s->c->bytecount -= s->size;
  }
}

static struct print_success_counts {
  ~print_success_counts() {
    if (0) {
      printf("Success_counts=");
      for (int i = 0; i < cpulimit; i++)
	if (cache_for_cpu[i].attempt_count)
	  printf(" %ld/%ld=%.0f%%", cache_for_cpu[i].success_count, cache_for_cpu[i].attempt_count,
		 100.0*(double)cache_for_cpu[i].success_count/(double)cache_for_cpu[i].attempt_count);
      printf("\n");
    }
  }
} psc;

static void* get_cached_small_malloc(size_t size) {
  binnumber_t bin = size_2_bin(size);
  bassert(bin < first_large_bin_number);
  // Still must access the cache atomically even though it's per processor.
  int p = getcpu() % cpulimit;
  __sync_fetch_and_add(&cache_for_cpu[p].attempt_count, 1);
  linked_list *r = atomic_load(&cache_for_cpu[p].c[bin].head);
  if (r == NULL) { return NULL; }
  get_cached_s v = {&cache_for_cpu[p].c[bin], bin_2_size(bin), NULL};
  atomically(&cache_for_cpu[p].lock,
	     predo_get_cached,
	     do_get_cached,
	     &v);
  __sync_fetch_and_add(&cache_for_cpu[p].success_count, 1);
  return v.result;
}

static void cached_small_free(void *ptr, binnumber_t bin) {
  int p = getcpu() % cpulimit;
  cached_objects *cfc = &cache_for_cpu[p].c[bin];
  if (atomic_load(&cfc->bytecount) > 4*1024*1024) {
    small_free(ptr);
  } else {
    linked_list *ll = (linked_list *)ptr;
    while (1) {
      linked_list *h  = cfc->head;
      ll->next        = h;
      if (__sync_bool_compare_and_swap(&cfc->head, h, ll)) break;
    }
    // It's OK for the count to be out of sync.  It's just an heurstic.
    __sync_fetch_and_add(&cfc->bytecount, bin_2_size(bin));
  }
}

// Three kinds of mallocs:
//   BIG, used for large allocations.  These are 2MB-aligned chunks.  We use BIG for anything bigger than a quarter of a chunk.
//   SMALL fit within a chunk.  Everything within a single chunk is the same size.
// The sizes are the powers of two (1<<X) as well as (1<<X)*1.25 and (1<<X)*1.5 and (1<<X)*1.75
extern "C" void *malloc(size_t size) {
  maybe_initialize_malloc();
  if (size >= max_allocatable_size) {
    errno = ENOMEM;
    return NULL;
  }
  void *result;
  if (size <= largest_small) {
    result = get_cached_small_malloc(size);
    if (!result) {
      result = small_malloc(size);
    }
  } else if (size <= largest_large) {
    result = large_malloc(size);
  } else {
    result = huge_malloc(size);
  }
  return result;
}

extern "C" void free(void *p) {
  maybe_initialize_malloc();
  if (p == NULL) return;
  chunknumber_t cn = address_2_chunknumber(p);
  binnumber_t bin = chunk_infos[cn].bin_number;
  if (bin < first_large_bin_number) {
    cached_small_free(p, bin);
  } else if (bin < first_huge_bin_number) {
    large_free(p);
  } else {
    huge_free(p);
  }
}

extern "C" void free_known_size(void *p, size_t size) {
  if (p == NULL) return;
  chunknumber_t cn = address_2_chunknumber(p);
  binnumber_t bin = chunk_infos[cn].bin_number;
  bassert(bin == size_2_bin(size));
  free(p);
}

extern "C" void* realloc(void *p, size_t size) {
  if (size >= max_allocatable_size) {
    errno = ENOMEM;
    return NULL;
  }
  if (p == NULL) return malloc(size);
  printf("realloc(%p, %ld)\n", p, size);
  abort();
}

extern "C" void* calloc(size_t number, size_t size) {
  void *result = malloc(number*size);
  size_t usable = malloc_usable_size(result);
  if (((uint64_t)result % pagesize == 0) &&
      (usable % pagesize == 0)) {
    madvise(result, usable, MADV_DONTNEED);
  } else {
    memset(result, 0, number*size);
  }
  return result;
}

extern "C" void* aligned_alloc(size_t alignment, size_t size) {
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
  binnumber_t bin = size_2_bin(size);
  while (bin_2_size(bin) & (alignment - 1)) {
    bin++;
  }
  return malloc(bin_2_size(bin)); // it looks like it happens to be the case that all multiples of powers of two are aligned to that power of two.
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
  binnumber_t bin = size_2_bin(size);
  while (bin_2_size(bin) & (alignment -1)) {
    bin++;
  }
  *ptr = malloc(bin_2_size(bin));
  return 0;
}

extern "C" size_t malloc_usable_size(const void *ptr) {
  chunknumber_t cn = address_2_chunknumber(ptr);
  binnumber_t bin = chunk_infos[cn].bin_number;
  return bin_2_size(bin);
}

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

#ifdef TESTING
int main() {
  initialize_malloc();
  test_hyperceil();
  test_size_2_bin();
  test_makechunk();
  test_huge_malloc();
  test_large_malloc();
  test_small_malloc();
}
#endif
