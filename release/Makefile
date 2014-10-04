ROOT     = ..
BUILD    = release
OPTFLAGS = -O3 -flto
UNITTESTS = # don't include unit tests
LINKMODE = dynamic
include ../Makefile.include

DLMALLOC_DIR = /home/bradley/malloc-implementations/dlmalloc/
test-malloc_test-dlmalloc: $(BLD)/test-malloc_test.o
	$(CXX) $< -L$(DLMALLOC_DIR) -ldlmalloc -Wl,-rpath,$(DLMALLOC_DIR) -pthread -o $@

HOARD_DIR = /home/bradley/malloc-implementations/Hoard/src
test-malloc_test-hoard:  $(BLD)/test-malloc_test.o
	$(CXX) $< -L$(HOARD_DIR) -lhoard -Wl,-rpath,$(HOARD_DIR) -pthread -o $@

new-malloc-test-libc: $(BLD)/new-malloc-test.o
	$(CXX) $< -pthread -o $@

new-malloc-test-dlmalloc: $(BLD)/new-malloc-test.o
	$(CXX) $< -L$(DLMALLOC_DIR) -ldlmalloc -Wl,-rpath,$(DLMALLOC_DIR) -pthread -o $@

new-malloc-test-hoard:  $(BLD)/new-malloc-test.o
	$(CXX) $< -L$(HOARD_DIR) -lhoard -Wl,-rpath,$(HOARD_DIR) -pthread -o $@

timings-simple:
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-libc -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-dlmalloc -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-hoard -w 2 -t 10
