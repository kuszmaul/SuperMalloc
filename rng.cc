#include "rng.h"

#if 0
// A hard-coded pseudorandom number generator
struct rng_state {
  int inited;
  uint64_t X;
};
__thread struct rng_state rng_state = {0,0};
  
uint64_t threadcount=0;

static const uint64_t X_init = 0xce3f12500545b241ul;
static const uint64_t a = 0xd31cd625d63ba689ul;
static const uint64_t b = 0x0a58ec3022b3b941ul;

uint64_t prandnum(void) {
  struct rng_state *r = &rng_state;
  if (!r->inited) {
    r->inited = 1;
    r->X = X_init + __sync_fetch_and_add(&threadcount, 1);
  }
  uint64_t newx = a*r->X + b;;
  r->X = newx;
  return newx;
}

#else

// This is copied out of Dave Dice's web log
//  https://blogs.oracle.com/dave/entry/a_simple_prng_idiom

static const uint64_t MIX = 0xdaba0b6eb09322e3ull;

static uint64_t Mix64 (uint64_t z) {
  z = (z ^ (z >> 32)) * MIX;
  z = (z ^ (z >> 32)) * MIX;
  return z ^ (z >> 32);
}

uint64_t prandnum() { 
  // Instead of incrementing by 1 we could also increment by a large
  // prime or by the MIX constant.

  // One clever idea from Dave Dice: We use the address of the
  // __thread variable as part of the hashed input, so that our random
  // number generator doesn't need to be initialized.

  // Make this aligned(64) so that we don't get any false sharing.
  // Perhaps __thread variables already avoid false sharing, but I
  // don't want to verify that.
  static __thread uint64_t __attribute__((aligned(64))) rv = 0;
  return Mix64 (reinterpret_cast<int64_t>(&rv) + (++rv));
}

#endif
