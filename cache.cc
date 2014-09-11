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
  linked_list *tail;
};

static const cached_objects empty_cached_objects = {0, NULL, NULL};

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

static const uint64_t per_cpu_cache_bytecount_limit = 1024*1024;
static const uint64_t thread_cache_bytecount_limit = 2*4096;

volatile unsigned int cache_lock;

static void* try_get_cached(cached_objects *co, uint64_t siz) {
  linked_list *result = co->head;
  if (result) {
    co->bytecount -= siz;
    co->head = result->next;
    // tail is the right value.
  }
  return result;
}
static void* try_get_cached_both(CacheForBin *cb,
				 uint64_t siz) {
  void *r = try_get_cached(&cb->co[0], siz);
  if (r) return r;
  return try_get_cached(&cb->co[1], siz);
}

static void predo_remove_a_cache_from_cpu(CacheForBin *cc,
					  cached_objects *co) {
  prefetch_write(cc);
  prefetch_write(co);
}

static bool do_remove_a_cache_from_cpu(CacheForBin *cc,
				       cached_objects *co) {
  if (cc->co[0].head) {
    *co = cc->co[0];
    cc->co[0] = empty_cached_objects;
  } if (cc->co[1].head) {
    *co = cc->co[1];
    cc->co[1] = empty_cached_objects;
  } else {
    *co = empty_cached_objects;
  }
  return true;
}

static void predo_add_a_cache_to_cpu(CacheForBin *cc,
				     /*const*/ cached_objects *co) // I wanted that to be "const cached_objects &co", but I couldn't make the type system happy.
{
  bassert(co->head != NULL);
  uint64_t bc0 = cc->co[0].bytecount;
  uint64_t bc1 = cc->co[1].bytecount;
  prefetch_write(cc);
  prefetch_read(co);
  if (bc0 != 0 && bc1 !=0) {
    if (bc0 <= bc1) {
      prefetch_write(cc->co[0].tail);
    } else {
      prefetch_write(cc->co[1].tail);
    }
  }
}

static bool do_add_a_cache_to_cpu(CacheForBin *cc,
				  /*const*/ cached_objects *co)  // I wanted that to be "const cached_objects &co", but I couldn't make the type system happy.
{
  // bassert(co->head != NULL);  This assert was done in the predo, and it won't have changd.
  uint64_t bc0 = cc->co[0].bytecount;
  uint64_t bc1 = cc->co[1].bytecount;
  if (bc0 == 0) {
    cc->co[0] = *co;
  } else if (bc1 == 0) {
    cc->co[1] = *co;
  } else if (bc0 <= bc1) {
    // add it to c0
    cc->co[0].tail->next = co->head;
    cc->co[0].tail = co->tail;
    cc->co[0].bytecount += co->bytecount;
  } else {
    cc->co[1].tail->next = co->head;
    cc->co[1].tail = co->tail;
    cc->co[1].bytecount += co->bytecount;
  }
  return true;
}


static void* try_get_cpu_cached(int processor,
				binnumber_t bin,
				uint64_t siz) {
  // Implementation notes:
  // 1) Atomically remove stuff from the cpu cache.
  // 2) Grab thread_cache_bytecount_limit worth of items of that list (which can be done without holding a lock, see we will have exclusive ownership of that list)
  // 3) Atomically put everything else back into the cpu cache (this requires maintaining tail in the cpu cache after all)
  // 4) Return the one object.

  CacheForBin *tc = &cache_for_thread.cb[bin];
  CacheForBin *cc = &cache_for_cpu[processor].cb[bin];

  // Step 1
  cached_objects my_co;
  atomically(&cache_lock,
	     predo_remove_a_cache_from_cpu,
	     do_remove_a_cache_from_cpu,
	     cc,
	     &my_co);

  // Step 2 (this doesn't need to be atomic)
  linked_list *result = my_co.head;
  if (!result) return NULL;
  my_co.bytecount -= siz;
  my_co.head = result->next;

  bool h0 = tc->co[0].head ;
  cached_objects *to_co = h0 ? &tc->co[1] : &tc->co[0];
  bassert(to_co->head == NULL);
  if (my_co.bytecount < thread_cache_bytecount_limit) {
    *to_co = my_co;
    // No need to put anything back.
  } else {
    to_co->head = my_co.head;
    uint64_t bytecount = siz;
    linked_list *ptr = my_co.head;
    while (bytecount < thread_cache_bytecount_limit) { // I don't really care if it goes one over, and I want to make sure *something* gets moved.
      bytecount += siz;
      bassert(ptr);
      ptr       = ptr->next;
    }
    bassert(ptr);
    to_co->tail = ptr;
    to_co->bytecount = bytecount;
    my_co.head       = ptr->next;
    my_co.bytecount -= bytecount;
    // This one doesn't have the option of failing.  It's possible that the CPU cache got really big while we were doing that stuff, but there's no point of trying to
    // put stuff into the global cache (it might be full too) and the prospect of freeing all those objects sounds unappetizingly slow.  Just let the cpu cache get too
    // big.
    atomically(&cache_lock,
	       predo_add_a_cache_to_cpu,
	       do_add_a_cache_to_cpu,
	       cc,
	       &my_co);
  }
  return result;
}

static void predo_get_global_cached(CacheForBin *cb,
				    GlobalCacheForBin *gb,
				    uint64_t siz __attribute__((unused))) {
  int n = atomic_load(&gb->n_nonempty_caches);
  if (n > 0) {
    prefetch_read(&gb->co[n-1]);
    prefetch_write(gb);
    prefetch_write(cb);
    // That's probably enough.  Prefetrching allthe other details looks tricky.
  }
}


static void* do_get_global_cached(CacheForBin *cb,
				  GlobalCacheForBin *gb,
				  uint64_t siz) {
  int n = gb->n_nonempty_caches;
  if (n > 0) {
    linked_list *result = gb->co[n-1].head;
    linked_list *result_next = result->next;
    if (result_next) {
      // if there was only one result in the list, then don't modify the cpu cache.
      cached_objects *co0 = &cb->co[0];
      cached_objects *co1 = &cb->co[1];
      cached_objects *co  = co0->bytecount < co1->bytecount ? co0 : co1;

      if (co->head == NULL) {
	co->tail = gb->co[n-1].tail;
      } else {
	gb->co[n-1].tail->next = co->head;
      }
      co->head = result_next;
      co->bytecount = gb->co[n-1].bytecount - siz;
    }
    gb->n_nonempty_caches = n-1;
    return result;
  } else {
    return 0;
  }
}

static void* try_get_global_cached(int processor,
				   binnumber_t bin,
				   uint64_t siz) {
  // Try moving stuff from a global cache to a cpu cache.  Grab one of the objects while we are there.
  return atomically(&cache_lock,
		    predo_get_global_cached,
		    do_get_global_cached,
		    &cache_for_cpu[processor].cb[bin],
		    &global_cache.gb[bin],
		    siz);
}

uint64_t global_cache_attempt_count = 0;
uint64_t global_cache_success_count = 0;

void* cached_malloc(size_t size)
// Effect: Try to the thread cache first.  Otherwise try the cpu cache
//   (move several things from the cpu cache to the thread cache if we
//   can do it efficiently), otherwise try the global cache (move a
//   whole chunk from the global cache to the cpu cache).
{
  binnumber_t bin = size_2_bin(size);
  bassert(bin < first_huge_bin_number);
  uint64_t siz = bin_2_size(bin);

  {
    cache_for_thread.attempt_count++;
    void *result = try_get_cached_both(&cache_for_thread.cb[bin],
				       siz);
    if (result) {
      cache_for_thread.success_count++;
      clog_command('a', result, size);
      return result;
    }
  }

  // Still must access the cache atomically even though it's per processor.
  int p = getcpu() % cpulimit;
  __sync_fetch_and_add(&cache_for_cpu[p].attempt_count, 1);

  {
    void *result = try_get_cpu_cached(p, bin, siz);
    if (result) {
      __sync_fetch_and_add(&cache_for_cpu[p].success_count, 1);
      clog_command('a', result, size);
      return result;
    }
  }

  {
    __sync_fetch_and_add(&global_cache_attempt_count, 1);
    void *result = try_get_global_cached(p, bin, siz);
    if (result) {
      __sync_fetch_and_add(&global_cache_success_count, 1);
      clog_command('a', result, siz);
      return result;
    }
  }
    
  // Didn't get a result.  Use the underlying alloc
  if (bin < first_large_bin_number) {
    void *result = small_malloc(size);
    clog_command('a', result, size);
    return result;
  } else {
    void *result = large_malloc(size);
    clog_command('a', result, size);
    return result;
  }
}

// This is not called atomically, it's only operating on thread cache
static bool try_put_cached(linked_list *obj, cached_objects *co, uint64_t size, uint64_t cache_size) {
  uint64_t bc = co->bytecount;
  if (bc < cache_size) {
    linked_list *h = co->head;
    obj->next = h;
    if (h == NULL) {
      co->bytecount = size;
      co->head = obj;
      co->tail = obj;
    } else {
      co->bytecount = bc+size;
      co->head = obj;
      // tail is already correct.
    }
    return true;
  } else {
    return false;
  }
}

// This is not called atomically, it's only operating on thread cache
static bool try_put_cached_both(linked_list *obj,
				CacheForBin *cb,
				uint64_t size,
				uint64_t cache_size) {
  if (try_put_cached(obj, &cb->co[0], size, cache_size)) return true;
  if (try_put_cached(obj, &cb->co[1], size, cache_size)) return true;
  return false;
}

static bool predo_try_put_into_cpu_cache_part(linked_list *obj,
					      cached_objects *tco,
					      cached_objects *cco) {
  uint64_t old_cco_bytecount = cco->bytecount;
  if (old_cco_bytecount < per_cpu_cache_bytecount_limit) {
    prefetch_write(obj);
    prefetch_write(tco->tail);
    prefetch_write(tco);
    prefetch_write(cco);
    return true;
  }
  return false;
}

static void predo_put_into_cpu_cache(linked_list *obj,
				     cached_objects *tco,
				     CacheForBin *cc,
				     uint64_t siz __attribute__((unused))) {
  if (predo_try_put_into_cpu_cache_part(obj, tco, &cc->co[0])) return;
  if (predo_try_put_into_cpu_cache_part(obj, tco, &cc->co[1])) return;
  return;
}

static bool try_put_into_cpu_cache_part(linked_list *obj,
					cached_objects *tco,
					cached_objects *cco,
					uint64_t siz) {
  uint64_t old_cco_bytecount = cco->bytecount;
  linked_list *old_cco_head  = cco->head;
  if (old_cco_bytecount < per_cpu_cache_bytecount_limit) {
    obj->next = tco->head;

    bassert(tco->tail != NULL);
    tco->tail->next = old_cco_head;

    cco->bytecount = old_cco_bytecount + tco->bytecount + siz ;
    cco->head      = obj;
    if (!old_cco_head) cco->tail = tco->tail;

    tco->bytecount = 0;
    tco->head = NULL;
    tco->tail = NULL;                                  // The tail can be left undefined if the head is NULL.
    
  }
  return false;
}
static bool do_put_into_cpu_cache(linked_list *obj,
				  cached_objects *tco,
				  CacheForBin *cc,
				  uint64_t siz) {
  // Always move stuff from 
  if (try_put_into_cpu_cache_part(obj, tco, &cc->co[0], siz)) return true;
  if (try_put_into_cpu_cache_part(obj, tco, &cc->co[1], siz)) return true;
  return false;
}

static bool try_put_into_cpu_cache(linked_list *obj,
				   int processor,
				   binnumber_t bin,
				   uint64_t siz)
// Effect: Move obj and stuff from a threadcache into a cpu cache, if the cpu has space.
// Requires: the threadcache has stuff in it.
{
  return atomically(&cache_lock,
		    predo_put_into_cpu_cache,
		    do_put_into_cpu_cache,
		    obj,
		    &cache_for_thread.cb[bin].co[0], // always move from bin 0
		    &cache_for_cpu[processor].cb[bin],
		    siz);
}

static void predo_put_into_global_cache(linked_list *obj,
					CacheForBin *cb,
					GlobalCacheForBin *gb,
					uint64_t siz __attribute__((unused))) {
  uint8_t gnum = atomic_load(&gb->n_nonempty_caches);
  if (gnum < global_cache_depth ) {
    prefetch_write(obj);
    prefetch_write(&gb->co[gnum]);
    prefetch_write(&cb->co[0]);
    prefetch_write(&gb->n_nonempty_caches);
  }
}

static bool do_put_into_global_cache(linked_list *obj,
				     CacheForBin *cb,
				     GlobalCacheForBin *gb,
				     uint64_t siz)
// Effect: if the first cpu cache is full and there's a free global cache, then move the first cpu cache and the object into the global cache.
{
  uint8_t gnum = gb->n_nonempty_caches;
  uint64_t old_bytecount = cb->co[0].bytecount;
  if (gnum < global_cache_depth && old_bytecount >= per_cpu_cache_bytecount_limit) {

    obj->next = cb->co[0].head;

    gb->co[gnum].bytecount = old_bytecount + siz;
    gb->co[gnum].head = obj;
    gb->co[gnum].tail = cb->co[0].tail;

    gb->n_nonempty_caches = gnum+1;

    cb->co[0].bytecount = 0;
    cb->co[0].head = NULL;
    cb->co[0].tail = NULL;                  // The tail can be left undefined if the head is NULL.
    return true;
  }
  return false;
}

static bool try_put_into_global_cache(linked_list *obj,
				      int processor,
				      binnumber_t bin,
				      uint64_t siz) {
  return atomically(&cache_lock,
		    predo_put_into_global_cache,
		    do_put_into_global_cache,
		    obj,
		    &cache_for_cpu[processor].cb[bin],
		    &global_cache.gb[bin],
		    siz);
}
				      

void cached_free(void *ptr, binnumber_t bin) {
  // What I want:
  //  If the threadcache is empty enough, add the object to the thread cache, and we are done.
  //  If the cpucache is empty enough, add everything in one of the threadcaches to the cpucache (including ptr), and we are done.
  //  Else if the global cache is empty enough, add everything from one of the cpucaches (including the ptr), and we are done.
  //  Else really free the pointer.
  clog_command('f', ptr, bin);
  bassert(bin < first_huge_bin_number);
  uint64_t siz = bin_2_size(bin);
  
  // No lock needed for this.
  if (try_put_cached_both(reinterpret_cast<linked_list*>(ptr),
			  &cache_for_thread.cb[bin],
			  siz,
			  thread_cache_bytecount_limit)) {
    return;
  }

  int p = getcpu() % cpulimit;
  
  if (try_put_into_cpu_cache(reinterpret_cast<linked_list*>(ptr),
			     p, bin, siz)) {
    return;
  }
			     
  if (try_put_into_global_cache(reinterpret_cast<linked_list*>(ptr),
				p, bin, siz)) {
    return;
  }

  // Finally must really do the work.
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
