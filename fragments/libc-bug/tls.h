#include <stddef.h>

class tl_stack {
  static const size_t STACK_SIZE = 1024 * 1024;
  size_t m_current_offset;
  char *m_stack;
 public:
  size_t get_free_space() const {
    return STACK_SIZE - m_current_offset;
  }
};

extern "C" size_t get_size();

