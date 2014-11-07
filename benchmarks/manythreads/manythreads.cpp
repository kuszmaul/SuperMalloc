#include <cassert>
#include <cstdlib>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <sys/resource.h>

static long getmaxrss(void) {
    struct rusage ru;
    int r = getrusage(RUSAGE_SELF, &ru);
    assert(r==0);
    return ru.ru_maxrss;;
}

static std::mutex only_one_at_a_time;

static void runit(int i) {
  int n_allocs = 10000;
  char **data = new char* [n_allocs];
  char **sdata = new char* [n_allocs];
  int scount=0;
  {
    std::lock_guard<std::mutex> lock(only_one_at_a_time);
    for (int i = 0; i< n_allocs; i++) {
      int siz = random()%4096;
      data[i] = (char*)malloc(siz);
      for (int j = 0; j < siz; j++) data[i][j] = j;
      if (i%16 == 0) {
	sdata[scount++] = (char*)malloc(siz);
	for (int j = 0; j < siz; j++) data[i][j] = j;
      }
    }
    for (int i = 0; i< n_allocs; i++) {
      free(data[i]);
    }
    delete [] data;
  }
  sleep(5);
  for (int i = 0; i < scount; i++) {
    free(sdata[i]);
  }
  delete [] sdata;
}

static int n_threads;

int main (int argc, char *argv[]) {
  assert(argc==2);
  n_threads = atoi(argv[1]);
  std::thread *threads = new std::thread[n_threads];
  for (int i = 0; i < n_threads; i++) {
    threads[i] = std::thread(runit, i);
  }
  for (int i = 0; i < n_threads; i++) {
    threads[i].join();
  }
  fprintf(stderr, "maxrss=%ld\n", getmaxrss());
  return 0;
}
