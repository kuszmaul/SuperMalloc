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

#endif

