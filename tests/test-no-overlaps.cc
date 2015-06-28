// Test to make sure that supermalloc doesn't allocate the same byte
// to more than one object.  See #26.
//
// The test does this:
//  Repeatedly Allocate and free objects:
//
//  When we allocate an object p, fill the ith byte of p with h(p,i)
//  where h is a hash function.
//
//  When we free an object, check its hash values.
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

// From Doug Lea and Guy Steele, via Dave Dice's blog.
static const uint64_t MIX = 0xdaba0b6eb09322e3ull;

static int64_t Mix64 (int64_t z) {
  z = (z ^ (uint64_t(z) >> 32)) * MIX;
  z = (z ^ (uint64_t(z) >> 32)) * MIX;
  return z ^ (uint64_t(z) >> 32);
}

static uint8_t hash(uint8_t *p, uint64_t i, uint64_t j) {
  return ((Mix64(Mix64(reinterpret_cast<uint64_t>(p))
		 + Mix64(i)
		 + Mix64(j)))
	  % 256);
}

static uint64_t gen_counter = 0;

struct pair {
  uint64_t size;
  uint8_t *p;
  uint64_t gen;
  pair(uint64_t size, uint8_t* p)
      : size(size)
      , p(p)
      , gen(gen_counter++)
  {
    for (uint64_t i = 0; i < size; i++) {
      p[i] = hash(p, i, gen);
    }
  }
  void check() {
    for (uint64_t i = 0; i < size; i++) {
      if (p[i] != hash(p, i, gen)) {
	printf(" size=%ld p=%p gen=%ld, found=%d expected=%d\n", size, p, gen, p[i], hash(p, i, gen));
      }
      assert(p[i] == hash(p, i, gen));
    }
  }
};

static const int n_to_alloc_per_pass = 1000;
static const int n_to_free_per_pass = n_to_alloc_per_pass/2;
static const int n_passes = 100;

static pair malloc_random_size()
// Effect: Pick a size so that we can pick small as well as large objects.
{
  uint64_t size_upper = 1;
  while (random()%2 == 0 && size_upper < 1ul<<27) size_upper*=2;
  if (random()%16 == 0) {
    return pair(size_upper, reinterpret_cast<uint8_t*>(aligned_alloc(size_upper, size_upper)));
  } else {
    uint64_t result = 1+(random() & (size_upper-1));
    return pair(result, reinterpret_cast<uint8_t*>(malloc(result)));
  }
}

static pair pop_random(std::vector<pair> *pairs) {
  assert(!pairs->empty());
  uint64_t i = random() % pairs->size();
  pair p = (*pairs)[i];
  (*pairs)[i] = (*pairs)[pairs->size() - 1];
  pairs->pop_back();
  return p;
}

int main (int argc, char *argv[] __attribute__((unused))) {
  assert(argc == 1);
  std::vector<pair> pairs;
  for (int pass = 0; pass < n_passes; pass++) {
    for (int anum = 0; anum < n_to_alloc_per_pass; anum++) {
      pair p = malloc_random_size();
      //printf("m(%p) size=%ld\n", p.p, p.size);
      pairs.push_back(p);
    }
    for (pair &pa : pairs) {
      pa.check();
    }
    for (int fnum = 0; fnum < n_to_free_per_pass; fnum++) {
      pair p = pop_random(&pairs);
      p.check();
      //printf("f(%p)\n", p.p);
      free(p.p);
    }
  }
  return 0;
}
