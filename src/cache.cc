#include <stdio.h>

#include "malloc_internal.h"
#include "atomically.h"
#include "generated_constants.h"
#include "bassert.h"
#include "thread-info.h"

thread_local struct thread_info thread_info;

#ifdef ENABLE_LOG_CHECKING
static void clog_command(char command, const void *ptr, size_t size);
#else
#define clog_command(a,b,c) ((void)0)
#endif

typedef bool ignore;

// Cached sched_getcpu costs about 3ns
// sched_getcpu costs about 23ns
// CPUID costs about 3ns
//   (Linux 3.15.10 on i7-4600U with turboboost disabled)
// In spite of all that, the cpuid instruction makes the code run far slower (possibly it disrupts the caches).

#if 0
static void cpuid(unsigned info, unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx)
{
    __asm__(
        "cpuid;"                                            /* assembly code */
        :"=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx) /* outputs */
        :"a" (info), "c"(0)                                 /* input: info into eax */
                                                            /* clobbers: none */
    );
}

int getcpu(void) {
  unsigned int a, b, c, d;
  cpuid(0xb, &a, &b, &c, &d);
  return d;
}

#elif 1
static uint32_t getcpu(void) {
  if ((thread_info.cached_cpu_count++)%16  ==0) { thread_info.cached_cpu = sched_getcpu(); if (0) printf("cpu=%d\n", thread_info.cached_cpu); }
  return thread_info.cached_cpu;
}
#elif 0
static uint32_t getcpu(void) {
  return sched_getcpu();
}
#endif

static const cached_objects empty_cached_objects = {0, NULL, NULL};

void cache_destructor() {
  //unsigned long recovered = 0;
  for (binnumber_t bin = 0 ; bin < first_huge_bin_number; bin++) {
    for (int j = 0; j < 2; j++) {
      //recovered += cache_for_thread.cb[bin].co[j].bytecount;
      linked_list *next;
      for (linked_list *head = thread_info.cache.cb[bin].co[j].head;
	   head;
	   head = next) {
	next = head->next;
	if (bin < first_large_bin_number) {
	  small_free(head);
	} else {
	  large_free(head);
	}
      }
    }
  }
  //printf("recovered %ld\n", recovered);
}

static void init_cache() {
}

static CacheForCpu cache_for_cpu[cpulimit];

static const int global_cache_depth = 8;

struct GlobalCacheForBin {
  uint8_t n_nonempty_caches __attribute__((aligned(64)));
  cached_objects co[global_cache_depth];
};

struct GlobalCache {
  GlobalCacheForBin gb[first_huge_bin_number];
};

static GlobalCache global_cache;

static const uint64_t per_cpu_cache_objectcount_limit = 128;
static const uint64_t thread_cache_objectcount_limit = 16;

lock_t cpu_cache_locks[cpulimit][first_huge_bin_number]; // these locks could less aligned, as long as the the first one for each cpu is aligned.
lock_t global_cache_locks[first_huge_bin_number];

static void* try_get_cached(cached_objects *co) {
  linked_list *result = co->head;
  if (result) {
    co->objectcount--;
    co->head = result->next;
    // tail is the right value.
  }
  return result;
}
static void* try_get_cached_both(CacheForBin *cb) {
  void *r = try_get_cached(&cb->co[0]);
  if (r) return r;
  return try_get_cached(&cb->co[1]);
}

#ifdef TESTING
static void assert_equal(const cached_objects *co, uint64_t objectcount, linked_list *h, linked_list *t) {
  bassert(co->objectcount == objectcount);
  bassert(co->head == h);
  if (co->head) {
    bassert(co->tail == t);
  }
}

static void test_try_get_cached_both()
{
  {
    CacheForBin c = {{{0,NULL,NULL},{0,NULL,NULL}}};
    void *r = try_get_cached_both(&c);
    bassert(r==NULL);
  }
  {
    linked_list item1 = {NULL};
    linked_list item2 = {&item1};
    CacheForBin c = {{{2, &item2, &item1},{0,NULL,NULL}}};
    void *r = try_get_cached_both(&c);
    bassert(r = reinterpret_cast<void*>(&item2));
    assert_equal(&c.co[0], 1, &item1, &item1);
    assert_equal(&c.co[1], 0, NULL, NULL);
    void *r2 = try_get_cached_both(&c);
    bassert(r2 = reinterpret_cast<void*>(&item1));
    assert_equal(&c.co[0], 0, NULL, NULL);
    assert_equal(&c.co[1], 0, NULL, NULL);
  }
  {
    linked_list item1 = {NULL};
    linked_list item2 = {&item1};
    CacheForBin c = {{{0,NULL,NULL},{2, &item2, &item1}}};
    void *r = try_get_cached_both(&c);
    bassert(r = reinterpret_cast<void*>(&item2));
    assert_equal(&c.co[1], 1, &item1, &item1);
    assert_equal(&c.co[0], 0, NULL, NULL);
    void *r2 = try_get_cached_both(&c);
    bassert(r2 = reinterpret_cast<void*>(&item1));
    assert_equal(&c.co[1], 0, NULL, NULL);
    assert_equal(&c.co[0], 0, NULL, NULL);
  }
}
#endif

static void predo_remove_a_cache_from_cpu(CacheForBin *cc,
					  cached_objects *co) {
  prefetch_write(cc);
  prefetch_write(co);
}

static ignore do_remove_a_cache_from_cpu(CacheForBin *cc,
				       cached_objects *co) {
  if (cc->co[0].head) {
    *co = cc->co[0];
    cc->co[0] = empty_cached_objects;
  } else if (cc->co[1].head) {
    *co = cc->co[1];
    cc->co[1] = empty_cached_objects;
  } else {
    *co = empty_cached_objects;
  }
  return true;
}

#ifdef TESTING
static void test_remove_a_cache_from_cpu() {
  linked_list item1= {NULL};
  linked_list item2= {NULL};
  CacheForBin c = {{{1024, &item1, &item1}, {1024, &item2, &item2}}};
  cached_objects co = {0,NULL,NULL};
  {
    predo_remove_a_cache_from_cpu(&c, &co);
    do_remove_a_cache_from_cpu(&c, &co);
    assert_equal(&co, 1024, &item1, &item1);
    assert_equal(&c.co[0],  0, NULL, NULL);
    assert_equal(&c.co[1], 1024, &item2, &item2);
  }
  {
    predo_remove_a_cache_from_cpu(&c, &co);
    do_remove_a_cache_from_cpu(&c, &co);
    assert_equal(&co, 1024, &item2, &item2);
    assert_equal(&c.co[0],  0, NULL, NULL);
    assert_equal(&c.co[1],  0, NULL, NULL);
  }
  {
    predo_remove_a_cache_from_cpu(&c, &co);
    do_remove_a_cache_from_cpu(&c, &co);
    assert_equal(&co,       0, NULL, NULL);
    assert_equal(&c.co[0],  0, NULL, NULL);
    assert_equal(&c.co[1],  0, NULL, NULL);
  }
}
#endif

static void predo_add_a_cache_to_cpu(CacheForBin *cc,
				     /*const*/ cached_objects *co) // I wanted that to be "const cached_objects &co", but I couldn't make the type system happy.
{
  bassert(co->head != NULL);
  uint64_t bc0 = cc->co[0].objectcount;
  uint64_t bc1 = cc->co[1].objectcount;
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

static ignore do_add_a_cache_to_cpu(CacheForBin *cc,
				    /*const*/ cached_objects *co)  // I wanted that to be "const cached_objects &co", but I couldn't make the type system happy.
// Effect:  co is a collection of cached objects (with a count and organized in a linked list)
//          cc is cache.
//          Add the objects from co to cc using O(1) time.
// Modifies: co
// ToDo: Google style guid calls for output parameters to follow input parameters.  cc should be after co.
{
  // bassert(co->head != NULL);  This assert was done in the predo, and it won't have changd.
  uint64_t oc0 = cc->co[0].objectcount;
  uint64_t oc1 = cc->co[1].objectcount;
  if (oc0 == 0) {
    cc->co[0] = *co;
  } else if (oc1 == 0) {
    cc->co[1] = *co;
  } else if (oc0 <= oc1) {
    // add it to c0
    cc->co[0].tail->next = co->head;
    cc->co[0].tail = co->tail;
    cc->co[0].objectcount += co->objectcount;
  } else {
    cc->co[1].tail->next = co->head;
    cc->co[1].tail = co->tail;
    cc->co[1].objectcount += co->objectcount;
  }
  return true;
}

#ifdef TESTING
static void test_add_a_cache_to_cpu() {
  {
    CacheForBin cc = {{{0,0,0},{0,0,0}}};
    linked_list item={0};
    cached_objects co = {1024, &item, &item};
    do_add_a_cache_to_cpu(&cc, &co);
    assert_equal(&cc.co[0], 1024, &item, &item);
    assert_equal(&cc.co[1],0,0,0);
    bassert(item.next == 0);
  }
  {
    linked_list item2;
    CacheForBin cc = {{{2048, &item2, &item2},{0,0,0}}};
    linked_list item;
    cached_objects co = {1024,&item,&item};
    do_add_a_cache_to_cpu(&cc, &co);
    assert_equal(&cc.co[0], 2048, &item2, &item2);
    assert_equal(&cc.co[1], 1024, &item,  &item);
  }
  {
    linked_list item3={0};
    linked_list item2={0};
    CacheForBin cc = {{{1024, &item2, &item2},{2048,&item3,&item3}}};
    linked_list item={0};
    cached_objects co = {1024,&item,&item};
    do_add_a_cache_to_cpu(&cc, &co);
    assert_equal(&cc.co[0], 2048, &item2, &item);
    bassert(item2.next  == &item);
    bassert(item.next == NULL);
    assert_equal(&cc.co[1], 2048, &item3,  &item3);
    bassert(item3.next == NULL);
  }
  {
    linked_list item3={0};
    linked_list item2={0};
    CacheForBin cc = {{{2048,&item3,&item3},{1024, &item2, &item2}}};
    linked_list item={0};
    cached_objects co = {1024,&item,&item};
    do_add_a_cache_to_cpu(&cc, &co);
    assert_equal(&cc.co[1], 2048, &item2, &item);
    bassert(item2.next  == &item);
    bassert(item.next == NULL);
    assert_equal(&cc.co[0], 2048, &item3,  &item3);
    bassert(item3.next == NULL);
  }
}
#endif

static void collect_objects_for_thread_cache(cached_objects *objects,
					     cached_objects *first_n_objects) {
  if (objects->objectcount < thread_cache_objectcount_limit) {
    *first_n_objects = *objects;
    *objects = empty_cached_objects;
  } else {
    first_n_objects->head = objects->head;
    linked_list *ptr =      objects->head;
    uint64_t objectcount = 1;
    while (objectcount < thread_cache_objectcount_limit) {
      objectcount++;
      bassert(ptr);
      ptr = ptr->next;
    }
    bassert(ptr);
    first_n_objects->tail = ptr;
    first_n_objects->objectcount = objectcount;
    objects->head = ptr->next;
    if (objects->head == NULL) objects->tail = NULL;
    objects->objectcount -= objectcount;
    ptr->next = NULL;
  }
}

__attribute__((optimize("unroll-loops")))
static void predo_fetch_one_from_cpu(CacheForBin *cc) {
  for (int i = 0; i < 2 ; i++) {
    linked_list *result = cc->co[i].head;
    if (result) {
      prefetch_write(&cc->co[i]);
      prefetch_read(result);
      return;
    }
  }
}

__attribute__((optimize("unroll-loops")))
static void* do_fetch_one_from_cpu(CacheForBin *cc) {
  for (int i = 0; i < 2; i++) {
    linked_list *result = cc->co[i].head;
    if (result) {
      cc->co[i].objectcount--;
      linked_list *next = result->next;
      cc->co[i].head = next;
      if (next == NULL) {
	cc->co[i].tail = NULL;
      }
      return result;
    }
  }
  return NULL;
}

static void* try_get_cpu_cached(int processor,
				binnumber_t bin) {

  if (use_threadcache) {
    // Implementation notes:
    // 1) Atomically remove stuff from the cpu cache.
    // 2) Grab thread_cache_bytecount_limit worth of items of that list (which can be done without holding a lock, see we will have exclusive ownership of that list)
    // 3) Atomically put everything else back into the cpu cache (this requires maintaining tail in the cpu cache after all)
    // 4) Return the one object.

    init_cache();
    CacheForBin *tc = &thread_info.cache.cb[bin];
    CacheForBin *cc = &cache_for_cpu[processor].cb[bin];

    // Step 1
    cached_objects my_co;
    atomically(&cpu_cache_locks[processor][bin], "remove_a_cache_from_cpu",
	       predo_remove_a_cache_from_cpu,
	       do_remove_a_cache_from_cpu,
	       cc,
	       &my_co);

    linked_list *result = my_co.head;
    if (!result) return NULL;
    my_co.head = result->next;
    my_co.objectcount--;

    // Step 2 (this doesn't need to be atomic)
    {
      cached_objects first_n_objects;
      collect_objects_for_thread_cache(&my_co, &first_n_objects);
    
      if (tc->co[0].head == NULL) {
	tc->co[0] = first_n_objects;
      } else {
	bassert(tc->co[1].head == NULL);
	tc->co[1] = first_n_objects;
      }
    }
    
    if (my_co.head != NULL) {
      // This one doesn't have the option of failing.  It's possible that
      // the CPU cache got really big while we were doing that stuff, but
      // there's no point of trying to put stuff into the global cache (it
      // might be full too) and the prospect of freeing all those objects
      // sounds unappetizingly slow.  Just let the cpu cache get too big.
      atomically(&cpu_cache_locks[processor][bin], "add_a_cache_to_cpu",
		 predo_add_a_cache_to_cpu,
		 do_add_a_cache_to_cpu,
		 cc,
		 &my_co);
    }
    return result;
  } else {
    // no threadcache.  Just try to get one thing out of the cpu cache and return it.
    return atomically(&cpu_cache_locks[processor][bin], "fetch_one_from_cpu",
		      predo_fetch_one_from_cpu,
		      do_fetch_one_from_cpu,
		      &cache_for_cpu[processor].cb[bin]);
  }
}

static void predo_get_global_cached(CacheForBin *cb,
				    GlobalCacheForBin *gb) {
  uint8_t n = atomic_load(&gb->n_nonempty_caches);
  if (n > 0) {
    linked_list *result = gb->co[n-1].head;
    if (result == NULL) return;  // result could be NULL if we see things in an invalid state
    linked_list *result_next = result->next;
    if (result_next) {
      cached_objects *co0 = &cb->co[0];
      cached_objects *co1 = &cb->co[1];
      cached_objects *co  = co0->objectcount < co1->objectcount ? co0 : co1;
      if (co->head == NULL) {
	linked_list *ignore __attribute__((unused)) = atomic_load(&gb->co[n-1].tail);
	prefetch_write(&co->tail); // already fetched as part of co->head
      } else {
	load_and_prefetch_write(&gb->co[n-1].tail->next);
      }
      prefetch_write(&co->head);
    }
    prefetch_write(&gb->n_nonempty_caches);
  }
}


static void* do_get_global_cached(CacheForBin *cb,
				  GlobalCacheForBin *gb)
// Effect: Try moving stuff from a global cache to a cpu cache.  Grab one of the objects while we are there.
//         Return an object if successful, otherwise return NULL.
//         This function is called while holding a lock, or inside a transaction.
{
  int n = gb->n_nonempty_caches;
  if (n > 0) {
    linked_list *result = gb->co[n-1].head;
    linked_list *result_next = result->next;
    if (result_next) {
      // if there was only one result in the list, then don't modify the cpu cache.
      cached_objects *co0 = &cb->co[0];
      cached_objects *co1 = &cb->co[1];
      cached_objects *co  = co0->objectcount < co1->objectcount ? co0 : co1;

      linked_list *co_head = co->head;
      if (co_head == NULL) {
	co->tail = gb->co[n-1].tail;
      } else {
	gb->co[n-1].tail->next = co_head;
      }
      co->head = result_next;
      co->objectcount = gb->co[n-1].objectcount - 1;
    }
    gb->n_nonempty_caches = n-1;
    return result;
  } else {
    return 0;
  }
}

static void* try_get_global_cached(int processor,
				   binnumber_t bin)
// Effect: Atomically try moving stuff from a global cache to a cpu cache.  Grab one of the objects while we are there.
//         Return an object if successful, otherwise return NULL.
{
  return atomically2(&global_cache_locks[bin], &cpu_cache_locks[processor][bin], "get_global_cached",
		     predo_get_global_cached,
		     do_get_global_cached,
		     &cache_for_cpu[processor].cb[bin],
		     &global_cache.gb[bin]);
}

#ifdef ENABLE_STATS
uint64_t global_cache_attempt_count = 0;
uint64_t global_cache_success_count = 0;
#endif

void* cached_malloc(binnumber_t bin)
// Effect: Try the thread cache first.  Otherwise try the cpu cache
//   (move several things from the cpu cache to the thread cache if we
//   can do it efficiently), otherwise try the global cache (move a
//   whole chunk from the global cache to the cpu cache).
{
  bassert(bin < first_huge_bin_number);
  uint64_t siz = bin_2_size(bin);

  if (use_threadcache) {
    init_cache();
#ifdef ENABLE_STATS
    cache_for_thread.attempt_count++;
#endif
    void *result = try_get_cached_both(&thread_info.cache.cb[bin]);
    if (result) {
#ifdef ENABLE_STATS
      cache_for_thread.success_count++;
#endif
      clog_command('a', result, siz);
      return result;
    }
  }

  // Still must access the cache atomically even though it's per processor.
  int p = getcpu() % cpulimit;
#ifdef ENABLE_STATS
  __sync_fetch_and_add(&cache_for_cpu[p].attempt_count, 1);
#endif

  {
    void *result = try_get_cpu_cached(p, bin);
    if (result) {
#ifdef ENABLE_STATS
      __sync_fetch_and_add(&cache_for_cpu[p].success_count, 1);
#endif
      clog_command('a', result, siz);
      return result;
    }
  }

  {
#ifdef ENABLE_STATS
    __sync_fetch_and_add(&global_cache_attempt_count, 1);
#endif
    void *result = try_get_global_cached(p, bin);
    if (result) {
#ifdef ENABLE_STATS
      __sync_fetch_and_add(&global_cache_success_count, 1);
#endif
      clog_command('a', result, siz);
      return result;
    }
  }
    
  // Didn't get a result.  Use the underlying alloc
  if (bin < first_large_bin_number) {
    void *result = small_malloc(bin);
    clog_command('a', result, siz);
    return result;
  } else {
    void *result = large_malloc(siz);
    clog_command('a', result, siz);
    return result;
  }
}

// This is not called atomically, it's only operating on thread cache
static bool try_put_cached(linked_list *obj, cached_objects *co, uint64_t cache_size_in_objects) {
  uint64_t oc = co->objectcount;
  if (oc < cache_size_in_objects) {
    linked_list *h = co->head;
    obj->next = h;
    if (h == NULL) {
      co->objectcount = 1;
      co->head = obj;
      co->tail = obj;
    } else {
      co->objectcount = oc+1;
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
				uint64_t cache_size_in_objects) {
  if (try_put_cached(obj, &cb->co[0], cache_size_in_objects)) return true;
  if (try_put_cached(obj, &cb->co[1], cache_size_in_objects)) return true;
  return false;
}

static bool predo_try_put_into_cpu_cache_part(linked_list *obj,
					      cached_objects *tco,
					      cached_objects *cco) {
  uint64_t old_cco_objectcount = cco->objectcount;
  if (old_cco_objectcount < per_cpu_cache_objectcount_limit) {
    obj->next = tco->head; // obj is private to the thread at this point, so we can write to it.
    prefetch_write(tco->tail);
    prefetch_write(tco);
    prefetch_write(cco);
    return true;
  }
  return false;
}

static void predo_put_into_cpu_cache(linked_list *obj,
				     cached_objects *tco,
				     CacheForBin *cc) {
  if (predo_try_put_into_cpu_cache_part(obj, tco, &cc->co[0])) return;
  if (predo_try_put_into_cpu_cache_part(obj, tco, &cc->co[1])) return;
  return;
}

static bool try_put_into_cpu_cache_part(linked_list *obj,
					cached_objects *tco,
					cached_objects *cco) {
  uint64_t old_cco_objectcount = cco->objectcount;
  linked_list *old_cco_head  = cco->head;
  if (old_cco_objectcount < per_cpu_cache_objectcount_limit) {
    obj->next = tco->head;

    bassert(tco->tail != NULL);
    tco->tail->next = old_cco_head;

    cco->objectcount = old_cco_objectcount + tco->objectcount + 1;
    cco->head      = obj;
    if (!old_cco_head) cco->tail = tco->tail;

    tco->objectcount = 0;
    tco->head = NULL;
    tco->tail = NULL;                                  // The tail can be left undefined if the head is NULL.
    
    return true;
  }
  return false;
}

static bool do_put_into_cpu_cache(linked_list *obj,
				  cached_objects *tco,
				  CacheForBin *cc) {
  // Always move stuff from 
  if (try_put_into_cpu_cache_part(obj, tco, &cc->co[0])) return true;
  if (try_put_into_cpu_cache_part(obj, tco, &cc->co[1])) return true;
  return false;
}

__attribute__((optimize("unroll-loops")))
static void predo_put_one_into_cpu_cache(linked_list *obj,
					 CacheForBin *cc) {
  for (int i = 0; i < 2; i++) {
    uint64_t old_objectcount = cc->co[i].objectcount;
    if (old_objectcount < per_cpu_cache_objectcount_limit) {
      linked_list *old_head = cc->co[i].head;
      obj->next = old_head; // obj is private, so we can write to it.
      prefetch_write(&cc->co[i].objectcount);
      return;
    }
  }
}

__attribute__((optimize("unroll-loops")))
static bool do_put_one_into_cpu_cache(linked_list *obj,
				      CacheForBin *cc) {
  for (int i = 0; i < 2; i++) {
    uint64_t old_objectcount = cc->co[i].objectcount;
    if (old_objectcount < per_cpu_cache_objectcount_limit) {
      linked_list *old_head = cc->co[i].head;
      if (old_head == NULL) {
	cc->co[i].tail = obj;
      }
      cc->co[i].head = obj;
      cc->co[i].objectcount = old_objectcount + 1;
      obj->next = old_head;
      return true;
    }
  }
  return false;
}

static bool try_put_into_cpu_cache(linked_list *obj,
				   int processor,
				   binnumber_t bin)
// Effect: Move obj and stuff from a threadcache into a cpu cache, if the cpu has space.
// Requires: the threadcache has stuff in it.
{
  if (use_threadcache) {
    init_cache();
    return atomically(&cpu_cache_locks[processor][bin], "put_into_cpu_cache",
		      predo_put_into_cpu_cache,
		      do_put_into_cpu_cache,
		      obj,
		      &thread_info.cache.cb[bin].co[0], // always move from bin 0
		      &cache_for_cpu[processor].cb[bin]);
  } else {
    return atomically(&cpu_cache_locks[processor][bin],  "put_one_into_cpu_cache",
		      predo_put_one_into_cpu_cache,
		      do_put_one_into_cpu_cache,
		      obj,
		      &cache_for_cpu[processor].cb[bin]);
  }
}

static void predo_put_into_global_cache(linked_list *obj,
					CacheForBin *cb,
					GlobalCacheForBin *gb) {
  uint8_t gnum = atomic_load(&gb->n_nonempty_caches);
  uint64_t old_objectcount = atomic_load(&cb->co[0].objectcount);
  if (gnum < global_cache_depth && old_objectcount >= per_cpu_cache_objectcount_limit) {
    obj->next = cb->co[0].head; // obj->next is private, so we can write to it.
    uint64_t ignore __attribute__((unused)) = atomic_load(&gb->co[gnum].objectcount);
    prefetch_write(&gb->co[gnum]);
    prefetch_write(&cb->co[0]);
    prefetch_write(&gb->n_nonempty_caches);
  }
}

static bool do_put_into_global_cache(linked_list *obj,
				     CacheForBin *cb,
				     GlobalCacheForBin *gb)
// Effect: if the first cpu cache is full and there's a free global cache, then move the first cpu cache and the object into the global cache.
{
  uint8_t gnum = gb->n_nonempty_caches;
  uint64_t old_objectcount = cb->co[0].objectcount;
  if (gnum < global_cache_depth && old_objectcount >= per_cpu_cache_objectcount_limit) {

    obj->next = cb->co[0].head;

    gb->co[gnum].objectcount = old_objectcount + 1;
    gb->co[gnum].head = obj;
    gb->co[gnum].tail = cb->co[0].tail;

    gb->n_nonempty_caches = gnum+1;

    cb->co[0].objectcount = 0;
    cb->co[0].head = NULL;
    cb->co[0].tail = NULL;                  // The tail can be left undefined if the head is NULL.
    return true;
  }
  return false;
}

static bool try_put_into_global_cache(linked_list *obj,
				      int processor,
				      binnumber_t bin) {
  return atomically2(&global_cache_locks[bin], &cpu_cache_locks[processor][bin], "put_into_global_cache",
		     predo_put_into_global_cache,
		     do_put_into_global_cache,
		     obj,
		     &cache_for_cpu[processor].cb[bin],
		     &global_cache.gb[bin]);
}
				      

void cached_free(void *ptr, binnumber_t bin) {
  // What I want:
  //  If the threadcache is empty enough, add the object to the thread cache, and we are done.
  //  If the cpucache is empty enough, add everything in one of the threadcaches to the cpucache (including ptr), and we are done.
  //  Else if the global cache is empty enough, add everything from one of the cpucaches (including the ptr), and we are done.
  //  Else really free the pointer.
  clog_command('f', ptr, bin);
  bassert(bin < first_huge_bin_number);
  
  // No lock needed for this.
  if (use_threadcache) {
    init_cache();
    if (try_put_cached_both(reinterpret_cast<linked_list*>(ptr),
			    &thread_info.cache.cb[bin],
			    thread_cache_objectcount_limit)) {
      return;
    }
  }

  int p = getcpu() % cpulimit;
  
  if (try_put_into_cpu_cache(reinterpret_cast<linked_list*>(ptr),
			     p, bin)) {
    return;
  }
			     
  if (try_put_into_global_cache(reinterpret_cast<linked_list*>(ptr),
				p, bin)) {
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

#ifdef TESTING
void test_cache_early() {
  test_try_get_cached_both();
  test_remove_a_cache_from_cpu();
  test_add_a_cache_to_cpu();
}
#endif
