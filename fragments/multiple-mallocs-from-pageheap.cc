/* Try to figure out whether allocating several objects from the page heap will help.
 * The model:
 *   We have a bunch of pages (P pages), each with 64 objects on them.
 *   We allocate out of the fullest page.
 *   We allocate 32P objects.
 *   Then repeatedly:
 *     Take the fullest non-full page (it has K free objects in it)
 *     Allocate all those objects.
 *     Free random other objects.
 *   Measure the average value of K.
 */

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>

static const uint64_t P = 1024;
static uint64_t page_maps[P];
static const uint64_t objects_per_page = 64;

static uint64_t sim_alloc()
// Effect: Simulate an allocation.
 //   Find the fullest non-full page and set a bit (indicating allocated).
// Implementation: Brute force search for the best page.
{
  uint64_t bestp = -1;
  int bestcount = -1; // 64 means all full, and is actually not good
  for (uint64_t p = 0; p < P; p++) {
    int thiscount = __builtin_popcountl(page_maps[p]);
    // thiscount is the number of allocated objects.
    if (thiscount == 64) {
      continue; // cannot use this page, since all are allocated.
    } else if (thiscount > bestcount) {
      bestp     = p;
      bestcount = thiscount;
    }
  }
  //  printf("bestp=%ld\n", bestp);
  //  printf("bestcount=%d\n", bestcount);
  assert(bestcount >= 0);
  uint64_t pm = page_maps[bestp];
  assert(~pm != 0); // We must have found something with a bit set
  int idx = __builtin_ctzl(~pm); //
  //  printf("idx=%d\n", idx);
  page_maps[bestp] = pm | (1ul << idx);
  //  printf("pm=%ld\n", pm);
  //  printf("new pm=%ld\n", page_maps[bestp]);
  return bestp*64+idx;
}

void sim_free(uint64_t free_me) {
  //printf("free 0x%lx (pm=%lx)\n", free_me, page_maps[free_me/64]);
  assert((page_maps[free_me/64] >> (free_me%64)) & 1);
  page_maps[free_me/64] &= ~(1ul << (free_me%64));
  //printf(" new pm=%lx\n", page_maps[free_me/64]);
}

std::vector<uint64_t> allocated;

uint64_t remove_random() {
  uint64_t idx = random() % allocated.size();
  uint64_t free_me = allocated[idx];
  allocated[idx] = allocated[allocated.size()-1];
  allocated.pop_back();
  return free_me;
}

int main (int argc, char *argv[] __attribute__((unused))) {
  assert(argc == 1);
  assert(objects_per_page / 8 == sizeof(page_maps[0]));
  for (uint64_t i = 0; i < (objects_per_page/2) * P; i++) {
    uint64_t v = sim_alloc();
    allocated.push_back(v);
  }
  if (0) {
    for (uint64_t i = 0; i < 10; i++) {
      uint64_t free_me = remove_random();
      printf("free(%ld)\n", free_me);
      sim_free(free_me);
      uint64_t v = sim_alloc();
      printf("alloc=%ld\n", v);
      allocated.push_back(v);
    }
  }
  uint64_t num_page_accesses = 0;
  uint64_t num_allocs = 0;
  for (uint64_t i = 0; i < 1000000; i++) {
    int count = 1;
    uint64_t v = sim_alloc();
    //printf("new group\n %2d: %lx\n", 0, v);
    allocated.push_back(v);
    num_page_accesses++;
    while (1) {
      uint64_t v2 = sim_alloc();
      if (v/objects_per_page != v2/objects_per_page) {
	sim_free(v2);
	break;
      }
      if (0) {
	if (count == 1) printf("new multi-alloc group\n %2d: %lx\n", 0, v);
	printf(" %2d: %lx\n", count, v2);
      }
      allocated.push_back(v2);
      count++;
    }
    for (int j = 0; j < count; j++) {
      uint64_t free_me = remove_random();
      sim_free(free_me);
      //printf(" f%lx\n", free_me);
    }
    num_allocs+=count;
  }
  printf("num_allocs        = %ld\n", num_allocs);
  printf("num_page_accesses = %ld\n", num_page_accesses);
  printf("ratio = %f\n", num_allocs/(double)num_page_accesses);
  return 0;
}
