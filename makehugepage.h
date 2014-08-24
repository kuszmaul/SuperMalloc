#ifndef MAKEHUGEPAGE_H
#define MAKEHUGEPAGE_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mmap_chunk_aligned_block(size_t n_chunks);

typedef uint64_t uintptr_t;
void *chunk_create(void);

void return_chunk_to_pool(void *chunk);

#ifdef TESTING
void test_chunk_create(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
