#include "print.h"
#include <unistd.h>

static void write_int_internal(int fd, int64_t number, int base) {
  if (number<base) {
    if (number<10) {
      char c='0'+number;
      write(fd, &c, 1);
    } else {
      char c='A'+number-10;
      write(fd, &c, 1);
    }
  } else {
    write_int_internal(fd, number/base, base);
    write_int_internal(fd, number%base, base);
  }
}

void write_int(int fd, int64_t number, int base) {
  if (number<0) {
    write(fd, "-", 1);
    write_int_internal(fd, -number, base); // This won't work for the must negative integer.
  } else {
    write_int_internal(fd, number,  base);
  }
  fsync(fd);
}

void write_uint(int fd, uint64_t number, int base) {
  if (number<(uint64_t)base) {
    write_int_internal(fd, number, base);
  } else {
    write_int_internal(fd, number/base, base); // guaranteed to be in the int64 range
    write_int_internal(fd, number%base, base);
  }
}

void write_ptr(int fd, void *p) {
  write(fd, "0x", 2);
  uint64_t v = (uint64_t)p;
  for (int i=0; i<16; i++) {
    write_int_internal(fd, (v >> (15-i))&0xf, 16);
  }
  fsync(fd);
}

static size_t my_strlen(const char *s) {
  size_t result = 0;
  while (*s++) result++;
  return result;
}

void write_string(int fd, const char *s) {
  write(fd, s, my_strlen(s));
  fsync(fd);
}
