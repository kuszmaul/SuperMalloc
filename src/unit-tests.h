#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

  void test_cache_early(void);
  void initialize_malloc(void);
  void test_hyperceil(void);
  void test_size_2_bin(void);
  void test_makechunk(void);
  void test_huge_malloc(void);
  void test_large_malloc(void);
  void test_small_malloc(void);
  void test_realloc(void);
  void test_malloc_usable_size(void);
  void test_object_base(void);

  void time_small_malloc(void);
#ifdef __cplusplus
}
#endif
