#include <stdio.h>

#include "malloc_internal.h"
#include "atomically.h"
#include "generated_constants.h"
#include "bassert.h"

#ifdef ENABLE_LOG_CHECKING
static void clog_command(char command, const void *ptr, size_t size);
#else
#define clog_command(a,b,c) ((void)0)
#endif


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
  //linked_list *tail;
};
struct CacheForBin {
  cached_objects co[2];
} __attribute__((aligned(64)));  // it's OK if the cached objects are on the same cacheline as the lock, but we don't want the cached objects to cross a cache boundary.  Since the CacheForBin has gotten to be 48 bytes, we might as well just align the struct to the cache.

struct CacheForCpu {
  uint64_t attempt_count, success_count;
  CacheForBin cb[first_huge_bin_number];
} __attribute__((aligned(64)));

static __thread CacheForCpu cache_for_thread ;

static CacheForCpu cache_for_cpu[cpulimit];

static const int global_cache_depth = 8;

struct GlobalCacheForBin {
  uint8_t n_nonempty_caches;
  cached_objects co[global_cache_depth];
};

struct GlobalCache {
  GlobalCacheForBin gb[first_huge_bin_number];
};

static GlobalCache global_cache;

volatile unsigned int cache_lock;

static inline linked_list* try_pop_from_cached_objects(cached_objects *co, uint64_t size) {
  linked_list *h = co->head;
  if (h) {
    co->head = h->next;
    co->bytecount -= size;
    return h;
  }
  return NULL;
}

static inline void predo_get_cached(CacheForBin *cb,
				    uint64_t size __attribute__((unused))) {
  {
    linked_list *h = atomic_load(&cb->co[0].head);
    if (h) {
      prefetch_read(h);
      prefetch_write(&cb->co[0]);
      return;
    }
  }
  {
    linked_list *h = atomic_load(&cb->co[1].head);
    if (h) {
      prefetch_read(h);
      prefetch_write(&cb->co[1]);
      return;
    }
  }
}

static inline void* do_get_cached(CacheForBin *cb,
				  uint64_t size) {
  {
    linked_list *h = try_pop_from_cached_objects(&cb->co[0], size);
    if (h) return h;
  }
  {
    linked_list *h = try_pop_from_cached_objects(&cb->co[1], size);
    if (h) return h;
  }
  return NULL;
}

static inline void predo_get_from_global_cache(CacheForBin *cb,
					       GlobalCacheForBin *gb,
					       uint64_t size __attribute__((unused))) {
  int n = gb->n_nonempty_caches;
  if (n > 0) {
    prefetch_read(&gb->co[n-1]);
    prefetch_write(&cb->co[0]);
    prefetch_write(&gb->n_nonempty_caches);
  }
}

static inline void* do_get_from_global_cache(CacheForBin *cb,
					     GlobalCacheForBin *gb,
					     uint64_t size) {
  int n = gb->n_nonempty_caches;
  if (n > 0 && cb->co[0].head==NULL) {
    cb->co[0] = gb->co[n-1];
    gb->n_nonempty_caches = n-1;
    linked_list *h = cb->co[0].head;;
    bassert(h != NULL);
    cb->co[0].head = h->next;
    cb->co[0].bytecount -= size;
    return h;
  }
  return NULL;
}

void* cached_malloc(size_t size)
// Effect: try the cache first, then try real small_malloc or large
{
  binnumber_t bin = size_2_bin(size);
  bassert(bin < first_huge_bin_number);

  if (   cache_for_thread.cb[bin].co[0].head != NULL
      || cache_for_thread.cb[bin].co[1].head != NULL) {
    void *result = do_get_cached(&cache_for_thread.cb[bin],
				 bin_2_size(bin));
    if (result) return result;
  }

  // Still must access the cache atomically even though it's per processor.
  int p = getcpu() % cpulimit;
  __sync_fetch_and_add(&cache_for_cpu[p].attempt_count, 1);
  // Don't bother doing a transaction if there's nothing in the caches.
  if (   !(atomic_load(&cache_for_cpu[p].cb[bin].co[0].head) ==NULL)
      || !(atomic_load(&cache_for_cpu[p].cb[bin].co[1].head) ==NULL)) {
    void *result = atomically(&cache_lock,
			      predo_get_cached,
			      do_get_cached,
			      &cache_for_cpu[p].cb[bin],
			      bin_2_size(bin));
    if (result) {
      bassert(chunk_infos[address_2_chunknumber(result)].bin_number == bin);
      __sync_fetch_and_add(&cache_for_cpu[p].success_count, 1);
      clog_command('a', result, size);
      return result;
    }
  }

  if (atomic_load(&global_cache.gb[bin].n_nonempty_caches) != 0) {
    void *result = atomically(&cache_lock, // we'd really like a finer grained lock and a course lock for the global cache.
			      predo_get_from_global_cache,
			      do_get_from_global_cache,
			      &cache_for_cpu[p].cb[bin],
			      &global_cache.gb[bin],
			      bin_2_size(bin));
    if (result) return result;
  }


  // Didn't get a result.  Use the underlying alloc
  if (bin < first_large_bin_number) {
    void *result = small_malloc(size);
    clog_command('b', result, size);
    return result;
  } else {
    void *result = large_malloc(size);
    clog_command('c', result, size);
    return result;
  }
}

static const uint64_t cache_bytecount_limit = 1024*1024;

static inline bool predo_try_put_cached(cached_objects *co) {
  linked_list *h = co->head;
  if (h) {
    prefetch_write(co);
    prefetch_read(h);
    return true;
  } else {
    return false;
  }
}

static inline void predo_put_cached(linked_list *obj,
				    CacheForBin *cb,
				    GlobalCacheForBin *gb,
				    uint64_t size __attribute__((unused))) {
  prefetch_write(obj);
  if (predo_try_put_cached(&cb->co[0])) return;
  if (predo_try_put_cached(&cb->co[1])) return;
  if (gb) {
    uint8_t gnum = gb->n_nonempty_caches;
    if (gnum < global_cache_depth) {
      prefetch_write(&gb->co[gnum]);
      prefetch_write(&cb->co[0]);
    }
  }
}


static inline bool try_put_cached(linked_list *obj, cached_objects *co, uint64_t size) {
  uint64_t bc = co->bytecount;
  if (bc < cache_bytecount_limit) {
    linked_list *h = co->head;
    obj->next = h;
    if (h == NULL) {
      co->bytecount = size;
      co->head = obj;
      //co->tail = obj;
    } else {
      co->bytecount = bc+size;
      co->head = obj;
    }
    return true;
  } else {
    return false;
  }
}

static inline bool do_put_cached(linked_list *obj,
				 CacheForBin *cb,
				 GlobalCacheForBin *gb,
				 uint64_t size) {
  if (try_put_cached(obj, &cb->co[0], size)) return true;
  if (try_put_cached(obj, &cb->co[1], size)) return true;
  if (gb) {
    uint8_t gnum = gb->n_nonempty_caches;
    if (gnum < global_cache_depth) {
      gb->co[gnum] = cb->co[0];
      gb->n_nonempty_caches = gnum+1;
      cb->co[0].head = NULL;
      cb->co[0].bytecount = 0;
      return true;
    }
  }
  return false;
}

void cached_free(void *ptr, binnumber_t bin) {
  clog_command('f', ptr, bin);
  bassert(bin < first_huge_bin_number);
  uint64_t siz = bin_2_size(bin);
  {
    // No lock needed for this.
    bool did_put = do_put_cached(reinterpret_cast<linked_list*>(ptr),
				 &cache_for_thread.cb[bin],
				 NULL,
				 siz);
    if (did_put) return;
  }

  int p = getcpu() % cpulimit;
  {
    bool did_put = atomically(&cache_lock,
			      predo_put_cached,
			      do_put_cached,
			      reinterpret_cast<linked_list*>(ptr),
			      &cache_for_cpu[p].cb[bin],
			      reinterpret_cast<GlobalCacheForBin*>(NULL),
			      siz);
    if (did_put) return;
  }
  {
    bool did_put = atomically(&cache_lock,
			      predo_put_cached,
			      do_put_cached,
			      reinterpret_cast<linked_list*>(ptr),
			      &cache_for_cpu[p].cb[bin],
			      &global_cache.gb[bin],
			      siz);
    if (did_put) return;
  }
  

  if (bin < first_large_bin_number) {
    small_free(ptr);
  } else {
    large_free(ptr);
  }
}

#ifdef ENABLE_STATS
void print_cache_stats() {
  printf("Success_counts=");
  for (int i = 0; i < cpulimit; i++)
    if (cache_for_cpu[i].attempt_count)
      printf(" %ld/%ld=%.0f%%", cache_for_cpu[i].success_count, cache_for_cpu[i].attempt_count,
	     100.0*(double)cache_for_cpu[i].success_count/(double)cache_for_cpu[i].attempt_count);
  printf("\n");
}
#endif

#ifdef ENABLE_LOG_CHECKING
static const int clog_count_limit = 10000000;
static int clog_count=0;
static struct logentry {
  char command;
  const void* ptr;
  size_t size;
} clog[clog_count_limit];

static void clog_command(char command, const void *ptr, size_t size) {
  int i = __sync_fetch_and_add(&clog_count, 1);
  if (i < clog_count_limit) {
    clog[i].command = command;
    clog[i].ptr     = ptr;
    clog[i].size    = size;
  } if (i == clog_count_limit) {
    printf("Log overflowed, I dealt with that by truncating the log\n");
  }
}

void check_log_cache() {
  printf("clog\n");
  for (int i = 0; i < clog_count; i++) {
    printf("%c %p %ld\n", clog[i].command, clog[i].ptr, clog[i].size);
  }
}
#endif
