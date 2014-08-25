#ifndef BASSERT_H
#define BASSERT_H

// We define our own assert macro, which is always a function, so that we can get rid of the missed branches reported by gcov.

#include <stdio.h>
#include <stdlib.h>

#ifdef COVERAGE
extern void bassert_f(int test, const char *pred, const char *file, int line, const char *fun);
#else
static inline  void bassert_f(int test, const char *pred, const char *file, int line, const char *fun) __attribute((unused));
static inline  void bassert_f(int test, const char *pred, const char *file, int line, const char *fun) {
  if (!test) {
    fprintf(stderr, "assertion failed: %s in %s at %s:%d\n", pred, fun, file, line);
    abort();;
  }
}
#endif

#define bassert(e) bassert_f(e, #e, __FILE__, __LINE__, __FUNCTION__)
#ifdef assert
#warning assert is deprecated, use bassert
#else
#define assert dont_use_assert_do_use_bassert
#endif




#endif
