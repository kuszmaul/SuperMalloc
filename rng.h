#ifndef RNG_H
#define RNG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t prandnum(void); // a threaded pseudo-random number generator.

#ifdef __cplusplus
}
#endif

#endif
