#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#include "malloc_internal.h"
#include "bassert.h"
#include "atomically.h"
#include "generated_constants.h"

#ifdef TESTING
#include <string.h>
#endif

static size_t addr_getoffset(void *p) {
  return ((uintptr_t)p) % chunksize;
}

void* mmap_size(size_t size) {
  void *r = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  bassert(r!=NULL);
  bassert(r!=MAP_FAILED);
  return r;
}

static void unmap(void *p, size_t size) {
  if (size>0) {
    int r = munmap(p, size);
#ifdef TESTING
    if (0 && r!=0) {
      fprintf(stderr, "Failure doing munmap(%p, %ld) error=%d\n", p, size, errno);
      abort();
    }
#endif
    bassert(r==0);
  }
}


static void *chunk_create_slow(size_t n_chunks) {
  size_t total_size = (1+n_chunks)*chunksize - pagesize;
  void *m = mmap_size(total_size);
  //printf("%s:%d m=%p\n", __FILE__, __LINE__, m);
  size_t m_offset = addr_getoffset(m);
  //printf("%s:%d offset=%lu\n", __FILE__, __LINE__, m_offset);
  size_t leading_useless = chunksize-m_offset;
  //printf("%s:%d leading_useless=%lu\n", __FILE__, __LINE__, leading_useless);
  unmap(m, leading_useless);
  void *final_m = ((char*)m) + leading_useless;
  //printf("%s:%d unmapping %p + %lu\n", __FILE__, __LINE__, (char*)final_m + chunksize, m_offset-pagesize);
  unmap((char*)final_m + n_chunks*chunksize, m_offset - pagesize);
  //printf("%s:%d returning %p\n", __FILE__, __LINE__, final_m);
  //((char*)final_m)[0] = '0';
  //((char*)final_m)[chunksize-1] = '0';
  return final_m;
}

void *mmap_chunk_aligned_block(size_t n_chunks)
// Effect: Return a pointer to a chunk.
//   The chunk is demapped zero-fill-on-demand.
//   The chunk might map as either a huge page or a lot of little
//    pages: the exact behavior isn't specified.  Use
//    madvise(MADV_HUGEPAGE) or MADV_NOHUGEPAGE to force the behavior you want.
//   We want this to be nonblocking, and we don't want to do a lot of crazy extra work if we
//    happen to create an extra chunk.
{
  // Quoting from jemalloc's chunk_mmap.c:
  /***
     * Ideally, there would be a way to specify alignment to mmap() (like
     * NetBSD has), but in the absence of such a feature, we have to work
     * hard to efficiently create aligned mappings.  The reliable, but
     * slow method is to create a mapping that is over-sized, then trim the
     * excess.  However, that always results in one or two calls to
     * pages_unmap().
     *
     * Optimistically try mapping precisely the right amount before falling
     * back to the slow method, with the expectation that the optimistic
     * approach works most of the time.
     */

  void * r = mmap_size(n_chunks*chunksize);
  if (addr_getoffset(r)!=0) {
    // Do it the slow way.
    unmap(r, n_chunks*chunksize);
    return chunk_create_slow(n_chunks);
  } else {
    //printf("%s:%d returning %p\n", __FILE__, __LINE__, r);
    return r;
  }
}

void test_makechunk(void) {
  {
    void *v = mmap_size(4096);
    bassert(v!=0);
    unmap(v, 4096);
    unmap(NULL, 0);
  }

  {
    void *v = mmap_chunk_aligned_block(1);
    bassert(v!=0);
    bassert(((uint64_t)v) % chunksize == 0);
    munmap(v, 1*chunksize);
  }
}
