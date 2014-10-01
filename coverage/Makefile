COVERAGE = -fprofile-arcs -ftest-coverage -DCOVERAGE
OPTFLAGS = -O0

C_CXX_FLAGS = -W -Wall -Werror $(OPTFLAGS) -ggdb -pthread -fPIC -mrtm $(COVERAGE)
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS) -std=c11
CPPFLAGS = -I..  -DTESTING

TESTS = aligned_alloc calloc posix_memalign test-malloc_test unit-tests 
default: $(TESTS)
run: $(TESTS)
	./unit-tests
	./aligned_alloc
	./calloc
	./posix_memalign
	SUPERMALLOC_TRANSACTIONS=1 ./unit-tests
	SUPERMALLOC_TRANSACTIONS=0 ./unit-tests
	SUPERMALLOC_PREDO=0 ./unit-tests
	SUPERMALLOC_PREDO=1 ./unit-tests
	SUPERMALLOC_THREADCACHE=0 ./unit-tests
	SUPERMALLOC_THREADCACHE=1 ./unit-tests
LIBTARGETS = malloc makechunk rng huge_malloc large_malloc small_malloc cache bassert footprint stats
$(TESTS): $(patsubst %,%.o,$(LIBTARGETS))
%.o: ../%.cc
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $^ -c -o $@
%.o: ../tests/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -c -o $@
malloc:
	$(CXX) $(CXXFLAGS) $(patsubst %, ../%.cc, $(LIBTARGETS)) -o $@
%: %.o
	$(CXX) $(CXXFLAGS) $^ -o $@


clean:
	rm -f *.o *.gcda *.gcno *.gcov $(TESTS)
gcov:
	gcov -bc $(patsubst %,../%.cc,$(LIBTARGETS)) -o . -s ..
