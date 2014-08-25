#ifndef MALLOC_CONSTANTS_H
#define MALLOC_CONSTANTS_H

#ifdef TESTING
#include <assert.h>
#include <stdio.h>
#endif

#include <stdint.h>

const uint64_t pagesize = 4096;
const uint64_t log_chunksize = 21;
const uint64_t chunksize = 1ul<<log_chunksize;

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
  if (0) printf("hyperceil(%ld)==%ld\n", a, r);
  return r;
}
static inline int lg_of_power_of_two(uint64_t a)
// Effect: Return the log_2(a).
// Requires: a is a power of two.
{
#ifdef TESTING
  assert((a & (a-1))==0);
#endif
  return __builtin_ctz(a);
}

static inline uint64_t chunk_number_of_address(void *a) {
  // Given an address anywhere in a chunk, convert it to a chunk number from 0 to 1<<27
  uint64_t au = (uint64_t)a;
  uint64_t am = au/chunksize;
  uint64_t result = am%(1ul<<27);
  return result;
}

// We keep a table of all the chunks for record keeping.
// Since the chunks are 2MB (21 bits) and the current address space of x86_64 processors is only 48 bits (256 TiB) \cite[p.120]{AMD12b}
// that means there can be at most 2^{27} chunks (that's 128 million chunks.)   We simply allocate a direct-mapped table for them all.
// We take the chunk's beginning address P, shift it as P>>21 in a sign-extended fashion.  Then add 2^26 and we have a table.
// Most of this table won't end up mapped.

extern struct chunk_info {
  uint32_t bin_number; // encodes how big the objects are in the chunk.
} *chunk_infos; // I want this to be an array of length [1u<<27], but that causes link-time errors.  Instead initialize_malloc() mmaps something big enough.

// Functions that are separated into various files.
void* huge_malloc(uint64_t size);
#ifdef TESTING
void test_huge_malloc(void);
#endif


typedef uint32_t chunknumber_t;

const unsigned int log_max_chunknumber = 27;
const chunknumber_t null_chunknumber = 0;

// We allocate chunks using only powers of two.  We don't bother with
// a buddy system to coalesce chunks, instead we just demap chunks
// that are no longer in use.  Each power of two, K, gets a linked
// list starting with free_chunks[K], which is a chunk number (we use
// 0 for the null chunk number).  The linked list employs the
// chunk_infos[] array to form the links.

extern chunknumber_t free_chunks[log_max_chunknumber];

#endif

