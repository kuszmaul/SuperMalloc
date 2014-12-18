#include "tls.h"
static __thread tl_stack local_stack;
  
extern "C" size_t get_size() {
  return local_stack.get_free_space();
};
  
