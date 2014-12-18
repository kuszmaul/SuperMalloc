#include "tls.h"

static __thread int tl_variable;
  
size_t get_size() {
  return tl_variable;
};
  
