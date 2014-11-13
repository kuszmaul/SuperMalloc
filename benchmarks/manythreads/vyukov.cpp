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


const int n_allocs = 50000;
const int n_threads = 200;

pthread_cond_t handoff_conditions[n_threads];
pthread_mutex_t handoff_mutexes[n_threads];
bool handoff[n_threads];
char *data[n_threads][n_allocs];

static void runit(int i) {
  if (i>0) {
    pthread_mutex_lock(&handoff_mutexes[i]);
    while(!handoff[i]) {
      pthread_cond_wait(&handoff_conditions[i], &handoff_mutexes[i]);
    }
    pthread_mutex_unlock(&handoff_mutexes[i]);
    for (int j = 0; j < n_allocs; j++) {
      free(data[i-1][j]);
    }
  }
  if (i+1 < n_threads) {
    for (int j = 0; j < n_allocs; j++) {
      int siz = random()%4096;
      data[i][j] = (char*)malloc(siz);
      for (int k = 0; k < siz; k++) {
	data[i][j][k] = k;
      }
    }
    handoff[i+1] = true;
    pthread_mutex_lock(&handoff_mutexes[i+1]);
    pthread_cond_signal(&handoff_conditions[i+1]);
    pthread_mutex_unlock(&handoff_mutexes[i+1]);
  }
  sleep(5);
}

int main (int argc, char *argv[] __attribute__((unused))) {
  assert(argc==1);
  std::thread *threads = new std::thread[n_threads];
  for (int i = 0; i < n_threads; i++) {
    pthread_cond_init(&handoff_conditions[i], NULL);
    pthread_mutex_init(&handoff_mutexes[i], NULL);
    handoff[i]=false;
  }
  for (int i = 0; i < n_threads; i++) {
    threads[i] = std::thread(runit, i);
  }
  for (int i = 0; i < n_threads; i++) {
    threads[i].join();
  }
  fprintf(stderr, "maxrss=%ld\n", getmaxrss());
  return 0;
}
