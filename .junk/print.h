#ifndef PRINT_H
#define PRINT_H

#include <stdint.h> // not inttypes.h since we don't do printf in this library.

#ifdef __cplusplus
extern "C" {
#endif

// We cannot use printf in this library (since *we* implement malloc), so we provide these simpler functions
// These functions are not buffered, and they may be slow (since they may include an fsync().

void write_int(int fd, int64_t number, int base);
void write_uint(int fd, uint64_t number, int base);
void write_ptr(int fd, void *p);
void write_string(int fd, const char *s);
#ifdef __cplusplus
}
#endif

#endif // PRINT_H
