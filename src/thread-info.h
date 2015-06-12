/* Put all the thread-specific data into a single class so that we can be sure to destruct it in the right order. */

struct linked_list {
  linked_list *next;
};

extern void cache_destructor();

struct cached_objects {
  uint64_t bytecount __attribute__((aligned(32)));
  linked_list *head;
  linked_list *tail;
};

struct CacheForBin {
  cached_objects co[2];
} __attribute__((aligned(64)));  // it's OK if the cached objects are on the same cacheline as the lock, but we don't want the cached objects to cross a cache boundary.  Since the CacheForBin has gotten to be 48 bytes, we might as well just align the struct to the cache.

struct CacheForCpu {
#ifdef ENABLE_STATS
  uint64_t attempt_count, success_count;
#endif
  CacheForBin cb[first_huge_bin_number];
} __attribute__((aligned(64)));

struct thread_info {
  uint32_t cached_cpu, cached_cpu_count;
  CacheForCpu cache;
 thread_info()
  : cached_cpu(0)
  , cached_cpu_count(0)
  {
  }
  ~thread_info() {
    cache_destructor();
  }
};

extern thread_local thread_info thread_info;

