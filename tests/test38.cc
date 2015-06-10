// Make sure that free() actually returns memory for huge blocks.
// See issue #38.

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
  const uint64_t size = 16 * 2 * 1024 * 1024;
  char *p = new char [size];
  for (uint64_t i = 0; i < size; i++) {
    p[i] = i%256;
  }
  const int vsize = (size+4095)/4096 -1;
  unsigned char *v = new unsigned char [vsize];
  char *p2 = reinterpret_cast<char*>(reinterpret_cast<uint64_t>(p+4095) & ~4095);
  uint64_t size2 = ((p+size-p2))&~4095;
  printf("p =%p size =%ld\np2=%p size2=%ld\n", p, size, p2, size2);
  {
    int r = mincore(p2,
		    size2,
		    v);
    if (r!=0) printf("r=%d errno=%d (%s)\n", r, errno, strerror(errno));
    assert(r==0);
  }
  for (int i = 0; i < vsize; i++) {
    assert(v[i]&1);
  }
  delete [] p;
  {
    int r = mincore(p2,
		    size-4096,
		    v);
    if (r!=0) printf("r=%d errno=%d (%s)\n", r, errno, strerror(errno));
    assert(r==0);
  }
  for (int i = 0; i < vsize; i++) {
    assert(!(v[i]&1));
  }



  delete [] v;
}
