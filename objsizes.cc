#include <stdint.h>
#include <stdio.h>

#include <algorithm>

int main () {
  const char *name = "GENERATED_CONSTANTS_H";
  printf("#ifndef %s\n", name);
  printf("#define %s\n", name);
  const uint64_t pagesize = 4096;
  const uint64_t overhead = 64;
  const int log_chunksize = 21;
  uint64_t slot_size = 1;
  printf("static const size_t slot_size = %ld;\n", slot_size);
  printf("static const int page_size = %ld;\n", pagesize);
  printf("typedef uint32_t binnumber_t;\n");
  printf("static struct { uint32_t object_size; uint32_t objects_per_page; } static_bin_info[] __attribute__((unused)) = {\n");
  printf("// pagesize = %ld bytes\n",          pagesize);
  printf("// overhead = %ld bytes per page\n", overhead);
  printf("// small objects:\n");
  printf("  /*objsize,*/ //   bin   wastage\n");
  int bin = 0;
  const int max_small = 512;
  uint64_t largest_small = 0;
  for (uint64_t i = 8; i < max_small; i*=2) {
    for (uint64_t j = 4; j <= 7; j++) {
      uint64_t objsize = (i*j)/4;
      if (objsize >= max_small) break;
      uint64_t n_objects_per_page = (pagesize-overhead)/objsize;
      uint64_t wasted = pagesize - overhead - n_objects_per_page * objsize;
      largest_small = std::max(largest_small, objsize);
      printf(" {%4lu, %3lu},  //   %3d      %3ld\n", objsize, n_objects_per_page, bin++, wasted);
    }
  }
  printf("// medium objects:\n");
  uint64_t largest_medium = 0;
  int  first_medium_bin = bin;
  for(uint64_t n_objects_per_page = 8; n_objects_per_page > 1; n_objects_per_page--) {
    uint64_t objsize = (pagesize-overhead)/n_objects_per_page;
    uint64_t wasted  = pagesize - overhead - n_objects_per_page * objsize;
    largest_medium = std::max(largest_medium, objsize);
    printf(" {%4lu, %3lu},  //   %3d       %3ld\n", objsize, n_objects_per_page, bin++, wasted);
  }
  printf("// large objects (page allocated):\n");
  int first_large_bin = bin;
  for (uint64_t log_allocsize = 12; log_allocsize < log_chunksize; log_allocsize++) {
    printf(" {1ul<<%2ld, 1}, //   %3d\n", log_allocsize, bin++);
  }
  printf("// huge objects (chunk allocated) start  at this size.\n");
  printf(" {1ul<<%d, 1}};// %3d\n", log_chunksize, bin++);
  printf("static const size_t largest_small         = %lu;\n", largest_small);
  printf("static const size_t largest_medium        = %lu;\n", largest_medium);
  printf("static const size_t largest_large         = %lu;\n", 1ul<<(log_chunksize-1));
  printf("static const size_t chunk_size            = %lu;\n", 1ul<<log_chunksize);
  printf("static const binnumber_t first_medium_bin_number = %u;\n", first_medium_bin);
  printf("static const binnumber_t first_large_bin_number = %u;\n", first_large_bin);
  printf("static const binnumber_t first_huge_bin_number   = %u;\n", bin-1);
  //  printf("static const uint64_t    slot_size               = %u;\n", slot_size);
  printf("#endif\n");
  return 0;
}
