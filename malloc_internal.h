#ifndef MALLOC_CONSTANTS_H
#define MALLOC_CONSTANTS_H

#ifdef TESTING
#include <stdio.h>
#include "bassert.h"

#endif

#include <stdint.h>
#include <sys/types.h>


const uint64_t pagesize = 4096;
const uint64_t log_chunksize = 21;
const uint64_t chunksize = 1ul<<log_chunksize;
const uint64_t cacheline_size = 64;
const uint64_t cachelines_per_page = pagesize/cacheline_size;

// We exploit the fact that these are the same size in chunk_infos, which is a union of these two types.
typedef uint32_t chunknumber_t;
typedef uint32_t binnumber_t;

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
  return __builtin_ctz(a);
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

// We keep a table of all the chunks for record keeping.
// Since the chunks are 2MB (21 bits) and the current address space of x86_64 processors is only 48 bits (256 TiB) \cite[p.120]{AMD12b}
// that means there can be at most 2^{27} chunks (that's 128 million chunks.)   We simply allocate a direct-mapped table for them all.
// We take the chunk's beginning address P, shift it as P>>21 in a sign-extended fashion.  Then add 2^26 and we have a table.
// Most of this table won't end up mapped.

extern struct chunk_info {
  union {
    binnumber_t bin_number;   // Encodes how big the objects are in the chunk.
    chunknumber_t next; // Forms a linked list.
  };
} *chunk_infos; // I want this to be an array of length [1u<<27], but that causes link-time errors.  Instead initialize_malloc() mmaps something big enough.

// Functions that are separated into various files.
void* huge_malloc(uint64_t size);
void huge_free(void* ptr);
#ifdef TESTING
void test_huge_malloc();
#endif


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

#ifdef TESTING
void test_makechunk();
#endif

void *large_malloc(size_t size);
void large_free(void* ptr);
#ifdef TESTING
void test_large_malloc();
#endif

void add_to_footprint(int64_t delta);
int64_t get_footprint();

void *small_malloc(size_t size);
void small_free(void* ptr);
#ifdef TESTING
void test_small_malloc();
#endif

extern bool use_threadcache;
void* cached_malloc(size_t size);
void cached_free(void *ptr, binnumber_t bin);

#ifdef TESTING
void test_cache_early();
#endif

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

#endif

