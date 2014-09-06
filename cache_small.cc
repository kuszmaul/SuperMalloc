#include <stdio.h>

#include "malloc_internal.h"
#include "atomically.h"
#include "generated_constants.h"
#include "bassert.h"

static __thread uint32_t cached_cpu, cached_cpu_count;
static uint32_t getcpu(void) {
  if ((cached_cpu_count++)%2048  ==0) { cached_cpu = sched_getcpu(); if (0) printf("cpu=%d\n", cached_cpu); }
  return cached_cpu;
}

struct linked_list {
  linked_list *next;
};

struct cached_objects {
  uint64_t bytecount;
  linked_list *head;
};
struct CacheForCpu {
  volatile unsigned int lock;
  uint64_t attempt_count, success_count;
  cached_objects c[first_large_bin_number] __attribute__((aligned(sizeof(cached_objects)))); // it's OK if the cached objects are on the same cacheline as the lock, but we don't want the cached objects to cross a cache boundary.
} __attribute__((aligned(64)));

CacheForCpu cache_for_cpu[cpulimit];

static void predo_get_cached(struct cached_objects *c,
			     uint64_t size __attribute__((unused))) {
  linked_list *h = c->head;
  if (h != NULL) {
    prefetch_write(c);
    prefetch_read(h);
  }
}
static void* do_get_cached(struct cached_objects *c,
			   uint64_t size) {
  linked_list *h = c->head;
  if (h != NULL) {
    c->head = h->next;
    c->bytecount -= size;
  }
  return h;
}

void* cached_small_malloc(size_t size)
// Effect: try the cache first, then try small_malloc
{
  binnumber_t bin = size_2_bin(size);
  bassert(bin < first_large_bin_number);
  // Still must access the cache atomically even though it's per processor.
  int p = getcpu() % cpulimit;
  __sync_fetch_and_add(&cache_for_cpu[p].attempt_count, 1);
  linked_list *r = atomic_load(&cache_for_cpu[p].c[bin].head);
  if (r == NULL) { return small_malloc(size); }
  void *result = atomically(&cache_for_cpu[p].lock,
			    predo_get_cached,
			    do_get_cached,
			    &cache_for_cpu[p].c[bin],
			    bin_2_size(bin));
  if (result) {
    __sync_fetch_and_add(&cache_for_cpu[p].success_count, 1);
    return result;
  } else {
    return small_malloc(size);
  }
}

void cached_small_free(void *ptr, binnumber_t bin) {
  int p = getcpu() % cpulimit;
  cached_objects *cfc = &cache_for_cpu[p].c[bin];
  if (atomic_load(&cfc->bytecount) > 4*1024*1024) {
    small_free(ptr);
  } else {
    linked_list *ll = (linked_list *)ptr;
    while (1) {
      linked_list *h  = cfc->head;
      ll->next        = h;
      if (__sync_bool_compare_and_swap(&cfc->head, h, ll)) break;
    }
    // It's OK for the count to be out of sync.  It's just an heurstic.
    __sync_fetch_and_add(&cfc->bytecount, bin_2_size(bin));
  }
}

// This is run for it's destructor at the end of the program.
static struct print_success_counts {
  ~print_success_counts() {
    if (0) {
      printf("Success_counts=");
      for (int i = 0; i < cpulimit; i++)
	if (cache_for_cpu[i].attempt_count)
	  printf(" %ld/%ld=%.0f%%", cache_for_cpu[i].success_count, cache_for_cpu[i].attempt_count,
		 100.0*(double)cache_for_cpu[i].success_count/(double)cache_for_cpu[i].attempt_count);
      printf("\n");
    }
  }
} psc;
