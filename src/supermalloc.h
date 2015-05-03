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
void* realloc(void *p, size_t size) __THROW;

// non_standard API
size_t malloc_usable_size(const void *ptr);

#define HAS_MALLOC_BASE_AND_BOUND 1
void _malloc_base_and_bound(const void *ptr, void **base, size_t *size);
// Effect: Given a pointer into a malloced object, determine the size
//  of the object (returned in *size) and the beginning of the object
//  (returned in *base)
//
// Note: The base might not be the the exact same pointer returned by
// malloc(), since malloc() sometimes adds random offsets to reduce
// cache associativity problems.  The pointer returned by malloc will
// be inside the range base..base+size, however.  Also, the base *can*
// be used to call free().
//
// Also, we want this to work if ptr points just off the end of the
// object, which means the object must be allocated to be at least one
// byte larger so that we can distinguish between that and pointing at
// the next object.  We need a mode that allocates that extra byte.
// (And don't forget not to include that extra byte in the malloc_usable_size).
//
// If HAS_MALLOC_BASE_AND_BOUND is 0 then it does nothing

#ifdef __cplusplus
}
#endif

#endif
