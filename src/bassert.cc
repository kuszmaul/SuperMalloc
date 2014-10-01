#include "bassert.h"

#ifdef COVERAGE
void bassert_f(bool test, const char *pred, const char *file, int line, const char *fun) {
  if (!test) {
    fprintf(stderr, "assertion failed: %s in %s at %s:%d\n", pred, fun, file, line);
    abort();;
  }
}
#endif
