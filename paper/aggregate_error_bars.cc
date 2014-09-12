// Effect: Read from standard input a data file.  
//  The first column is the key, the rest of the columns are data.
//  For each unique key print the key, and expand each of the columns into three: average min max
//  Data is separated by spaces.

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

static void put_substring(const char *str, int start_inclusive, int end_exclusive) {
  for (int i = start_inclusive; i < end_exclusive; i++) {
    putchar(str[i]);
  }
}
static long parse_int(const char *str, char **newstr) {
  errno = 0;
  long result = strtol(str, newstr, 10);
  assert(*newstr != str);
  assert(errno == 0);
  return result;
}

class field {
 public:
  int n;
  long sum;
  long min, max;
  field(): n(0), sum(0) {
  }
  void add_value(long v) {
    sum += v;
    if (n == 0) {
      min = v;
      max = v;
    } else {
      min = std::min(min, v);
      max = std::max(max, v);
    }
    n++;
  }
};

int main(int argc __attribute__((unused)), char *argv[]  __attribute__((unused))) {
  char *line = 0;
  size_t bufsize = 0;
  int n_data_columns;
  {
    ssize_t l = getline(&line, &bufsize, stdin);
    assert(l>0);
    std::vector<int> offsets;
    for (int i = 0; i < l; i++) {
      if (line[i] == ' ' || line[i] == '\n') {
	offsets.push_back(i);
      }
    }
    put_substring(line, 0, offsets[0]);
    for (size_t i = 1; i < offsets.size(); i++) {
      printf(" "); put_substring(line, offsets[i-1]+1, offsets[i]); printf("_avg");
      printf(" "); put_substring(line, offsets[i-1]+1, offsets[i]); printf("_min");
      printf(" "); put_substring(line, offsets[i-1]+1, offsets[i]); printf("_max");
    }
    printf("\n");
    n_data_columns = offsets.size()-1;
  }
  typedef std::vector<field> row;
  std::map<long, row> data;
  while (1) {
    ssize_t l = getline(&line, &bufsize, stdin);
    if (l < 0) break;
    char *ptr = line;
    int k = parse_int(ptr, &ptr);
    if (data.count(k) == 0) {
      data[k] = row(n_data_columns);
    }
    for (int i = 0; i < n_data_columns; i++) {
      assert(ptr[0] == ' ');
      long v = parse_int(ptr+1, &ptr);
      data[k][i].add_value(v);
    }
    assert(*ptr=='\n');
  }
  for (auto entry : data) {
    printf("%ld", entry.first);
    for (field f : entry.second) {
      printf(" %f %ld %ld", static_cast<double>(f.sum)/f.n, f.min, f.max);
    }
    printf("\n");
  }
  free(line);
  return 0;
}

