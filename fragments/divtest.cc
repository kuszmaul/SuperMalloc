#include <cassert>
#include <cstdint>
#include "division-generated.cc"
int main (int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
  for (uint32_t i = 0; i<(1u<<31); i++) {
    assert(div13(i) == i/13);
  }
  return 0;
}
