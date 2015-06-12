#ifndef MALLOC_CONSTANTS_H
#define MALLOC_CONSTANTS_H

#ifdef TESTING
#include <stdio.h>
#endif
#include "bassert.h"

#include <stdint.h>
#include <sys/types.h>

const uint64_t pagesize = 4096;
const uint64_t log_chunksize = 21;
const uint64_t chunksize = 1ul<<log_chunksize;
const uint64_t cacheline_size = 64;
const uint64_t cachelines_per_page = pagesize/cacheline_size;

// We exploit the fact that these are the same size in chunk_infos, which is a union of these two types.
typedef uint32_t chunknumber_t;
typedef uint32_t bin_and_size_t;  // we encode the bin number as 7 bits low-order bits.  The size is encoded as
    //                                1 bit means the size is in 4K pages (0) or the size is in 2M pages (1)
    //                                24 bits is the size (in 4K or 2M pages )
typedef uint32_t binnumber_t;
typedef uint16_t objects_per_folio_t;
typedef uint16_t folios_per_chunk_t;

bin_and_size_t bin_and_size_to_bin_and_size(binnumber_t bin, size_t size);
static inline binnumber_t bin_from_bin_and_size(bin_and_size_t bnt) {
  bassert((bnt&127) != 0);
  return (bnt&127)-1;
}

static inline uint64_t ceil(uint64_t a, uint64_t b) {
  return (a+b-1)/b;
}
static inline uint64_t hyperceil(uint64_t a)
// Effect: Return the smallest power of two >= a.
{
  uint64_t r = ((a <= 1)
		? 1
		: 1ul<<(64 - __builtin_clzl(a-1)));
#ifdef TESTING
  if (0) printf("hyperceil(%ld)==%ld\n", a, r);
#endif
  return r;
}
static inline int lg_of_power_of_two(uint64_t a)
// Effect: Return the log_2(a).
// Requires: a is a power of two.
{
#ifdef TESTING
  bassert((a & (a-1))==0);
#endif
  return __builtin_ctzl(a);
}

static inline chunknumber_t address_2_chunknumber(const void *a) {
  // Given an address anywhere in a chunk, convert it to a chunk number from 0 to 1<<27
  uint64_t au = reinterpret_cast<uint64_t>(a);
  uint64_t am = au/chunksize;
  uint64_t result = am%(1ul<<27);
  return result;
}

static inline void* address_2_chunkaddress(const void *a) {
  return reinterpret_cast<void*>( reinterpret_cast<uint64_t>(a) & ~ (chunksize-1));
}

static inline uint64_t offset_in_chunk(const void *a) {
  return reinterpret_cast<uint64_t>(a) % chunksize;
}

static inline uint64_t pagenum_in_chunk(const void *a) {
  return offset_in_chunk(a)/pagesize;
}
static inline uint64_t offset_in_page(const void *a) {
  return reinterpret_cast<uint64_t>(a) % pagesize;
}

static inline bool is_power_of_two(uint64_t x) {
  return (x&(x-1))==0;
}

void* object_base(void *ptr);
// Effect: if we are passed a pointer into the middle of an object, return the beginning of the object.
// Requires: the pointer must be on the same  chunk as the beginning of the object.

// We keep a table of all the chunks for record keeping.
// Since the chunks are 2MB (21 bits) and the current address space of x86_64 processors is only 48 bits (256 TiB) \cite[p.120]{AMD12b}
// that means there can be at most 2^{27} chunks (that's 128 million chunks.)   We simply allocate a direct-mapped table for them all.
// We take the chunk's beginning address P, shift it as P>>21 in a sign-extended fashion.  Then add 2^26 and we have a table.
// Most of this table won't end up mapped.

extern struct chunk_info {
  union {
    bin_and_size_t bin_and_size;
    chunknumber_t next; // Forms a linked list.
  };
} *chunk_infos; // I want this to be an array of length [1u<<27], but that causes link-time errors.  Instead initialize_malloc() mmaps something big enough.

// Functions that are separated into various files.
void* huge_malloc(uint64_t size);
void huge_free(void* ptr);

const unsigned int log_max_chunknumber = 27;
const chunknumber_t null_chunknumber = 0;

// We allocate chunks using only powers of two.  We don't bother with
// a buddy system to coalesce chunks, instead we just purge chunks
// that are no longer in use.  Each power of two, K, gets a linked
// list starting with free_chunks[K], which is a chunk number (we use
// 0 for the null chunk number).  The linked list employs the
// chunk_infos[] array to form the links.

extern chunknumber_t free_chunks[log_max_chunknumber];

void* mmap_chunk_aligned_block(size_t n_chunks); //

void *large_malloc(size_t size);
void large_free(void* ptr);

void add_to_footprint(int64_t delta);
int64_t get_footprint();

void *small_malloc(binnumber_t bin);
void small_free(void* ptr);

extern bool use_threadcache;
void* cached_malloc(binnumber_t bin);
void cached_free(void *ptr, binnumber_t bin);

const int cpulimit = 128;


#ifdef ENABLE_STATS
void print_cache_stats();

void print_bin_stats();
void bin_stats_note_malloc(binnumber_t b);
void bin_stats_note_free(binnumber_t b);

#else
static inline void print_cache_stats() {}

static inline void print_bin_stats() {}
static inline void bin_stats_note_malloc(binnumber_t b __attribute__((unused))) {}
static inline void bin_stats_note_free(binnumber_t b __attribute__((unused))) {}
#endif

#ifdef TESTING
#define IS_TESTING 1
#else
#define IS_TESTING 0
#endif

#ifdef ENABLE_LOG_CHECKING
void check_log_large();
#endif

struct large_object_list_cell {
  union {
    large_object_list_cell *next;
    uint32_t footprint;
  };
};

const uint32_t max_objects_per_folio = 2048; /* at most 2048 objects per folio. objsizes will check this when generated the constants. */ 
const uint32_t folio_bitmap_n_words = max_objects_per_folio/64; 

struct per_folio {
  per_folio *next __attribute__((aligned(64)));
  per_folio *prev;
  uint64_t inuse_bitmap[folio_bitmap_n_words]; // up to 512 objects (8 bytes per object) per page.  The bit is set if the object is in use.
};

#ifdef TESTING
#include "unit-tests.h"
#endif

static inline double tdiff(struct timespec *start, struct timespec *end) {
  return end->tv_sec - start->tv_sec + 1e-9*(end->tv_nsec - start->tv_nsec);
}

#endif

