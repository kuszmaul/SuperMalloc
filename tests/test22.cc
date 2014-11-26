// See what happens if I have a static allocator
// See issue #22 (atexit() hangs because a lock isn't ready)

#include "supermalloc.h"
#include <cstdio>

class Foo {
 public:
  void *a;
  Foo() {
    a = malloc(100);
  }
  ~Foo() {
    free(a);
  }
} foo;
  

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
  printf("a=%p\n", foo.a);
  return 0;
}
