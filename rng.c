#include "rng.h"
#include <stdbool.h>

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

