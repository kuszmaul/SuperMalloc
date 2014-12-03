#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unistd.h>

// For a portable version, see http://stackoverflow.com/questions/669438/how-to-get-memory-usage-at-run-time-in-c (Nadeau)
size_t get_current_rss() {
  long rss = 0L;
  FILE* fp = NULL;
  if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
    return (size_t)0L;      /* Can't open? */
  if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
  {
    fclose( fp );
    return (size_t)0L;      /* Can't read? */
  }
  fclose( fp );
  return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);
}

int main (int argc, char *argv[] __attribute__((unused))) {
  assert(argc==1);
  std::vector<void*> v;
  const int N=1000000;
  for (int i = 0; i < N; i++) {
    void * p = malloc(100);
    v.push_back(p);
    memset(p, 'c', 100);
  }
  size_t rss0 = get_current_rss();
  for (int i = 0; i < N; i++) {
    free(v[i]);
  }
  size_t rss1 = get_current_rss();
  printf("rss=%ld\n", rss0);
  printf("rss=%ld\n", rss1);
  assert(rss1 * 2 < rss0);
  return 0;
}
