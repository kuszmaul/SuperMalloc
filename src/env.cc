#include "atomically.h"
#include "has_tsx.h"
#include <cstring>
#include <stdio.h>

#define ENV_MODE_STRING "SUPERMALLOC_MODE"

mutex_mode_t mode;

static inline mutex_mode_t default_mode() {
  return have_TSX() ? MODE_TSX : MODE_PTHREAD_MUTEX;
}

__attribute__((constructor (101)))
void __setup_supermalloc_env() {
  const char *mode_str = getenv(ENV_MODE_STRING);

  if (NULL == mode_str) {
    mode = default_mode();
  } else {
    if (0 == std::strcmp("tsx", mode_str)) {
      if (!have_TSX()) {
	fprintf(stderr, "SuperMalloc: Warning: TSX mode unsupported on this"
		"machine.\n  This application will likely crash.\n");
      }
      mode = MODE_TSX;
    } else if (0 == std::strcmp("pthread_mutex", mode_str)) {
      mode = MODE_PTHREAD_MUTEX;
    } else {
      fprintf(stderr, "SuperMalloc: Warning: unknown mode '%s'.\n", mode_str);
      mode = default_mode();
    }
  }
}
