#ifndef MALLOC_CONSTANTS_H
#define MALLOC_CONSTANTS_H

#include <stdint.h>

const uint64_t pagesize = 4096;
const uint64_t log_chunksize = 21;
const uint64_t chunksize = 1ul<<log_chunksize;

typedef uint32_t binnumber_t;

static inline uint64_t ceil(uint64_t a, uint64_t b) {
  return (a+b-1)/b;
}

// We keep a table of all the chunks for record keeping.
// Since the chunks are 2MB and the current address space of x86_64 processors is only 48 bits (256 TiB) \cite[p.120]{AMD12b}
// that means there can be at most 2^{27} chunks (that's 128 million chunks.)   We simply allocate a direct-mapped table for them all.
// We take the chunk's beginning address P, shift it as P>>21 in a sign-extended fashion.  Then add 2^26 and we have a table.
// Most of this table won't end up mapped.

extern struct chunk_info {
  uint32_t bin_number; // encodes how big the objects are in the chunk.
} *chunk_infos; // I want this to be an array of length [1u<<27], but that causes link-time errors.  Instead initialize_malloc() mmaps something big enough.

#endif

