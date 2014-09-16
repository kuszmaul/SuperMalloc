#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

uint32_t ceil_log_2(uint64_t d) {
  uint32_t result = (__builtin_popcountl(d) == 1) ? 0 : 1;
  while (d>1) {
    result++;
    d = d>>1;
  }
  return result;
}

int main(int argc, char *argv[]) {
  assert(ceil_log_2(1)==0);
  assert(ceil_log_2(2)==1);
  assert(ceil_log_2(3)==2);
  assert(ceil_log_2(4)==2);
  assert(ceil_log_2(5)==3);
  assert(ceil_log_2(7)==3);
  assert(ceil_log_2(8)==3);
  assert(ceil_log_2(9)==4);

  for (int i = 1; i< argc; i++) {
    char *end;
    errno = 0;
    uint64_t d = strtoul(argv[i], &end, 10);
    assert(errno==0 && argv[i]!=end && *end==0);
    printf("// d=%lu\n", d);
    // Now generate code that can compute n/d for arbitrary 32-bit n.
    uint32_t p = ceil_log_2(d);
    printf("// p=ceil(log_2(%lu))=%u\n", d, p);
    uint64_t m = (d-1+(1ul<<(32+p)))/d;
    printf("// m=%lu = 0x%lx\n", m, m);
    printf("uint32_t div%lu(uint32_t n) {\n", d);
    printf("  return (static_cast<uint64_t>(n) * %luul) >> %d;\n", m, p+32);
    printf("}\n");
  }
}

uint32_t gcc_div13(uint32_t x) {
  return x/13;
}
