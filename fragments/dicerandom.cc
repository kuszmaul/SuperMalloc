// This is copied out of Dave Dice's web log
//  https://blogs.oracle.com/dave/entry/a_simple_prng_idiom

// Copying the content of the blog here (in case the blog page every
// evaporates....), Dave Dice writes:

//   The academic literature is rife with algorithms for pseudo-random
//   number generators (PRNGs). Typically, there's a trade-off between
//   performance and the quality of the distribution. In most cases I
//   need PRNGs to implement very lightweight Bernoulli trials for
//   randomized stress tests, benchmarks, or scalable probabilistic
//   counters. My budget is usually less that 100 cycles to generate a
//   uniformly distributed value. Marsaglia's xor-shift PRNG is one of
//   my personal favorites. If I need better quality I'll step up to
//   Ziff's four tap or Mersenne twister.
//
//   One variation of Marsaglia has only one word of state, a 4G-1
//   period, and requires just 3 shifts and 3 XOR operations to
//   generate a new value. 0 is an absorbing state that we avoid. See
//   MarsagliaNext(), below. Ignoring 0, the trajectory or stream of
//   values forms a cycle -- conceptually a ring. The initialization
//   and seeding operation should act to place different threads at
//   different positions on that ring. In a sense the ring is shared
//   by all threads but we start the threads at different
//   points. Unfortunately, we can sometimes find that 2 different
//   threads are at about the same position at about the same time
//   through simple bad luck, and thus generate similar streams of
//   values. (Longer PRNG cycles reduce the odds of such scenarios, of
//   course). Ideally, to avoid such inopportune and undesirable
//   behavior, each thread would have its own private ring of values.
//
//   A simple approach that is tantamount to giving each thread its
//   own ring -- trajectory of values -- is shown below in
//   NextRandom(). Hash32() is a hash function, which we'll describe
//   later. Note that we explicitly "color" the value passed into
//   Hash32() with the address of the thread-local PRNG state. Recall
//   that at any one time, such an address will be associated with at
//   most one thread, and is thus temporally unique. This approach
//   gives us additional independence over concurrently executing
//   threads. It also makes NextRandom() self-seeding -- explicit
//   initialization is not required.
//
//   The Hash32() hash function is the key to this particular PRNG,
//   and its implementation directly embodies the trade-off between
//   performance and the quality of the resulting
//   distribution. Conceptually, you could think of Hash32() as
//   representing a randomized cycle with a period of 4G. We might
//   implement Hash32() via a strong cryptographic hash, such as SHA-3
//   or MD5. Those hash functions would work perfectly well, but tend
//   to be high quality but somewhat expensive, so we'd prefer a
//   cheaper hash. Critically, we want the hash function to exhibit a
//   high degree of avalanche effect. (A good encryption function
//   could also be used as a hash operator). Some cheaper candidates
//   for the hash function include: MurmurHash ;CityHash ; FNV hash
//   family; siphash; and Jenkins hash.
//
//   Doug Lea and Guy Steele invented some of the best candidates for
//   our hash function: see Mix32() and Mix64() below. These are
//   relatively cheap but do well on avalanche tests and strike a
//   reasonable balance between quality and performance. Beware that
//   they're obviously not cryptographically strong. Mix32() and
//   Mix64() are related to mix functions found in
//   java.util.SplittableRandom

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#if 0
static int32_t MarsagliaNext () { 
  static __thread int32_t rv = 0 ; 
  if (rv == 0) rv = GenerateSeed() ; 
  int32_t v = rv ; 
  v ^= v << 6 ; 
  v ^= uint32_t(v) >> 21 ; 
  v ^= v << 7 ; 
  rv = v ; 
  return v ; 
}


static int NextRandom() { 
  // assumes a 32-bit environment : ILP32
  // Instead of incrementing by 1 we could also 
  // increment by a large prime or  by the MIX constant 
  // if we're using Mix32()  as our hash function.  
  static __thread int rv = 0 ;    
  return Hash32 (int(&rv) + (++rv)) ; 
}

// Mix32 and Mix64 are provided courtesy of Doug Lea and Guy Steele
static int32_t Mix32 (int32_t z) {
  static const int32_t MIX = 0x9abe94e3;
  // Another workable constant is 0xa0ca9b6b
  z = (z ^ (uint32_t(z) >> 16)) * MIX;
  z = (z ^ (uint32_t(z) >> 16)) * MIX;
  return z ^ (uint32_t(z) >> 16);
}

static int64_t Mix64 (int64_t z) {
  z = (z ^ (uint64_t(z) >> 32)) * MIX;
  z = (z ^ (uint64_t(z) >> 32)) * MIX;
  return z ^ (uint64_t(z) >> 32);
}

#endif

// I'm concerned about the mix of signed and unsigned values in the
// original code, and I would expect that if GLS and Doug Lea wrote
// that code, the unsigned/signed business is important.  But if I
// rewrite the code using unsigned values exclusively, the values seem
// to come out the same. -Bradley


static const uint64_t MIX = 0xdaba0b6eb09322e3ull;

static uint64_t Mix64 (uint64_t z) {
  z = (z ^ (z >> 32)) * (uint64_t)MIX;
  z = (z ^ (z >> 32)) * (uint64_t)MIX;
  return z ^ (z >> 32);
}

uint64_t prandnum() { 
  // assumes a 64-bit environment : ILP64
  // Instead of incrementing by 1 we could also 
  // increment by a large prime or  by the MIX constant 
  // if we're using Mix64()  as our hash function.  
  static __thread uint64_t rv = 0 ;    
  return Mix64 (reinterpret_cast<int64_t>(&rv) + (++rv)); 
}

// In fact, this code appears to optimize away completely, since the
// compiler is convinced that those assertions all succeed.

#if 0
static int64_t Mix64 (int64_t z) {
  z = (z ^ (uint64_t(z) >> 32)) * (int64_t)MIX;
  z = (z ^ (uint64_t(z) >> 32)) * (int64_t)MIX;
  return z ^ (uint64_t(z) >> 32);
}

int main (int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
  for (uint64_t i=0; i < 100000ul; i++) {
    assert((uint64_t)Mix64(i) == uMix64(i));
    assert((uint64_t)Mix64(i + (1ul<<63) ) == uMix64(i + (1ul<<63)));
  }
  return 0;
}
#endif
