#ifndef BMALLOC_H
#define BMALLOC_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void* malloc(size_t /*size*/) __THROW __attribute__((malloc));
void* calloc(size_t /*number*/, size_t /*size*/) __THROW __attribute__((malloc));
void free(void* /*ptr*/) __THROW;
void *aligned_alloc(size_t /*alignment*/, size_t /*size*/) __THROW;
int posix_memalign(void **memptr, size_t alignment, size_t size) __THROW;
void *memalign(size_t alignment, size_t size) __THROW;
void* realloc(void *p, size_t size) __THROW;

// non_standard API
size_t malloc_usable_size(const void *ptr);

#ifdef __cplusplus
}
#endif

#endif
