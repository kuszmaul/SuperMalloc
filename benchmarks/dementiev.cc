/* See Dementiev09, who claims that lazy commit produces nonscalable behavior for size=200K, and that tbbmalloc is better.
 * I think their analysis is wrong: the problem is something else.  strace indicates that libc malloc is
 *  not doing any system calls (not mmap() on allocation, neither munmap() nor madvise() on deallocation)
 *  so it cannot be that.  The problem, if there is one, is something else.
 * For size=20000K, things look just fine, even if I turn off the madvise inside huge_malloc.cc, it just doesn't matter much.
 */

#include <cstring>
#include <thread>
#include <sys/time.h>

const int n_iterations = 300;
const size_t size = 20000*1024;

void worker(void) {
  for (int iter = 0 ; iter < n_iterations; iter++) {
    void *p = malloc(size);
    memset(p, 0, size);
    free(p);
  }
}

int main(int argc __attribute__((unused)), const char *argv[] __attribute__((unused))) {
  printf("size=%ld n_iterations=%d\n", size, n_iterations);
  for (int tcount = 1; tcount < 64; tcount*=2) {
    std::thread *threads = new std::thread[tcount];
    struct timespec start,end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int tnum = 0; tnum < tcount; tnum++) {
      threads[tnum] = std::thread(worker);
    }
    for (int tnum = 0; tnum < tcount; tnum++) {
      threads[tnum].join();
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    delete[] threads;
    double rtime = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    printf("%2d threads %7.2fs runtime\n", tcount, rtime);
  }
}
