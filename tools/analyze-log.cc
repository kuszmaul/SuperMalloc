#include <map>
#include <cassert>
#include <cstdio>

std::map<long,char> mem;

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
  char *line = NULL;
  size_t n = 0;
  int count = 0;
  int erred = 0;
  while (getline(&line, &n, stdin)>=0) {
    char command;
    long ptr;
    int r = sscanf(line, "%c 0x%lx", &command, &ptr);
    assert(r==2);
    count++;
    if (command == 'a') {
      if (mem.count(ptr) == 0) {
	mem[ptr] = command;
      } else {
	printf("double malloc line %d\n", count);
	erred=1;
      }
    } else if (command = 'f') {
      if (mem.count(ptr) == 0) {
	printf("double free line %d\n", count);
	erred = 1;
      } else {
	mem.erase(ptr);
      }
    } else {
      assert(0);
    }
  }
  printf("count=%d\n", count);
  return erred;
}


