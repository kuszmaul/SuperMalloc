#include "makehugepage.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

#include "atomically.h"
#include "generated_constants.h"
#include "print.h"

#ifdef TESTING
#include <string.h>
#endif

static size_t addr_getoffset(void *p) {
  return ((uintptr_t)p) % chunksize;
}

void* mmap_size(size_t size) {
  void *r = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  assert(r!=NULL);
  assert(r!=MAP_FAILED);
  return r;
}

static void unmap(void *p, size_t size) {
  if (size>0) {
    int r = munmap(p, size);
    if (r!=0) {
      write_string(2, "Failure doing munmap(");
      write_ptr(2, p);
      write_uint(2, size, 10);
      write_string(2, ") error=");
      write_int(2, errno, 10);
    }
    assert(r==0);
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
//   The chunk is probably not actually mapped, but is zero-fill-on-demand (probably).
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

union chunk_union {
  union chunk_union *next;
  char chunk_data[chunksize];
};

struct the_chunk_list { // Put these in a struct together so they'll be on the same cache line.
  volatile unsigned int       lock __attribute((__aligned__(64)));
  union chunk_union *list;
};
static struct the_chunk_list tcl = {0,0};

struct cc_data {
  union chunk_union *u;
};

static void prepop_chunk(void *cc_data_v) {
  cc_data *cc = (cc_data*)cc_data_v;
  cc->u = tcl.list; // we want to write into the return result, and we want the list to be in cache.
  // But it's not good enough to simply do   __builtin_prefetch(&cc->u,    1, 3)
  // But we do want to convert the list to writeable if possible.
  __builtin_prefetch(&tcl.list, 1, 3); // we want to do a write (and a read) onto the chunk list.
}

static void pop_chunk(void *cc_data_v) {
  cc_data *cc = (cc_data*)cc_data_v;
  union chunk_union *head = tcl.list;
  cc->u = head;
  if (head) {
    tcl.list  = head->next;
  }
}

static void *chunk_get_from_pool(void) {
  struct cc_data d;
  atomically(&tcl.lock, prepop_chunk, pop_chunk, &d);
  return d.u;
}
  
static void prepush_chunk(void *cc_data_v) {
  cc_data *cc = (cc_data*)cc_data_v;
  cc->u->next = tcl.list;
  __builtin_prefetch(&tcl.list, 1, 3); // we want to do a write (and a read) onto the chunk list.
}

static void push_chunk(void *cc_data_v) {
  cc_data *cc = (cc_data *)cc_data_v;
  cc->u->next = tcl.list;
  tcl.list    = cc->u;
}

void return_chunk_to_pool(void *chunk) {
  struct cc_data d = {(chunk_union *)chunk};
  atomically(&tcl.lock, prepush_chunk, push_chunk, &d);
}

static void chunk_create_into_pool(void) {
  // Need to create a chunk
  return_chunk_to_pool(mmap_chunk_aligned_block(1));
}


void *chunk_create(void) {
  while (1) {
    void *p = chunk_get_from_pool();
    if (p) return p;
    chunk_create_into_pool();
  }
}

#ifdef TESTING
void test_chunk_create(void) {
  {
    void *p = chunk_get_from_pool();
    assert(p==0);
  }
  {
    chunk_create_into_pool();
    void *p = chunk_get_from_pool();
    assert(p);
    long pl = (long)p;
    assert(pl%chunksize == 0);
  }
}
#endif
