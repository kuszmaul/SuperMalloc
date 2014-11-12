/* Measure the cost of unmapping a page.

 */
#include <cassert>
#include <cstdio>
#include <ctime>
#include <sys/mman.h>

static double tdiff(const struct timespec &a, const struct timespec &b) {
  return (b.tv_sec - a.tv_sec) + 1e-9*(b.tv_nsec - a.tv_nsec);
}

const int N_iterations = 1000000;

static void doit(bool madvise_it, bool touch_it, char *page) {
  struct timespec start,end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  for (int i = 0; i < N_iterations; i++) {
    if (madvise_it) {
      int r = madvise(page, 4096, MADV_DONTNEED);
      assert(r==0);
    }
    if (touch_it) {
      __atomic_store_n(&page[0], 1, __ATOMIC_RELEASE);
    }
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  printf("%s %s & %8.1fns \\\\\n", madvise_it ? "yes-madvise" : " no-madvise", touch_it ? "yes-touch" : " no-touch", (tdiff(start, end)/N_iterations) * 1e9);

}

int main(int argc, char *argv[] __attribute__((unused))) {
  assert(argc == 1);
  char *page = (char*)(mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0));
  assert(page);
  doit(true, true, page);
  doit(true, false, page);
  doit(false, true, page);

  return 0;
}
