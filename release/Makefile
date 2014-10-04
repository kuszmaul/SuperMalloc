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
