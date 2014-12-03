#include <sys/mman.h>
#include <cassert>
#include <stdio.h>
#include <cstdint>

const int N=1ul<<28;
const uint64_t pagesize=4096ul;
struct allocation {
  void *p;
  uint64_t size;
} a[N];

int main(int argc, char *argv[] __attribute__((unused))) {
  assert(argc<=2);
  int count=0;
  int maxi = 0;
  uint64_t total_allocated = 0;
  if (argc==1) {
    for (int i = 0 ; i < N; i++) {
      size_t size = pagesize<<i;
      void *p = mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
      if (p == MAP_FAILED) {
	printf("%d failed\n", i);
	break;
      }
      printf("%d: %p\n", i, p);
      a[count++] = (struct allocation){p, size};
      maxi = i;
      total_allocated += size;
    }
    int i = maxi;
    while (i >= 0) {
      size_t size = pagesize<<i;
      void *p = mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
      if (p == MAP_FAILED) {
	printf("%d failed\n", i);
	i--;
      } else {
	printf("%d: %p\n", i, p);
	a[count++] = (struct allocation){p, size};
	total_allocated += size;
      }
    }
  } else {
    for (size_t i = 0 ; i < N; i++) {
      size_t size = 1ul<<21;
      void *p = mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
      if (p == MAP_FAILED) {
	printf("map %ld failed\n", i);
	break;
      }
      a[count++] = (struct allocation){p, size};
      total_allocated += size;
    }
  }
  for (int i = 0; i < count; i++) {
    int r = munmap(a[i].p, a[i].size);
    assert(r==0);
    if (0) printf("munamp(%p 1ul<<%d)\n",  a[i].p, __builtin_ctzl(a[i].size));
  }
  printf("Total allocated = %lx (%ld)\n", total_allocated, total_allocated);
  return 0;
}
