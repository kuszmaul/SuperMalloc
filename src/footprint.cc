/* Maintain a count of the footprint. */

#include <sched.h>

#include "malloc_internal.h"
#include "atomically.h"

static const int processor_number_limit = 128; // should be enough for now.
static uint64_t partitioned_footprint[processor_number_limit];

struct processor_id {
  int cpuid;
  int count;
} __attribute__((aligned(64)));

static const int prid_cache_time = 128;
static __thread processor_id prid;

static inline void check_cpuid(void) {
  if (prid.count++ % prid_cache_time == 0) {
    prid.cpuid = sched_getcpu() % processor_number_limit;
  }
}

void add_to_footprint(int64_t delta) {
  check_cpuid();
  __sync_fetch_and_add(&partitioned_footprint[prid.cpuid], delta);
}

int64_t get_footprint(void) {
  int64_t sum = 0;
  for (int i = 0; i < processor_number_limit; i++) {
    sum += partitioned_footprint[i];
    //sum += atomic_load(&partitioned_footprint[i]); // don't really care if we slightly stale answers.
  }
  return sum;
}
