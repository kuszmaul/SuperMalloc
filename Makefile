# COVERAGE = -fprofile-arcs -ftest-coverage -DCOVERAGE
OPTFLAGS = -O0 #-flto
C_CXX_FLAGS = -W -Wall -Werror $(OPTFLAGS) -ggdb -pthread -fPIC -mrtm $(COVERAGE)
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS) -std=c11

default: libsupermalloc.so malloc tests_default
tests_default: libsupermalloc.so
	cd tests;$(MAKE)

# While compiling malloc or any of its .o files, compile with -DTESTING
libsupermalloc.so: malloc.o makechunk.o rng.o huge_malloc.o large_malloc.o small_malloc.o bassert.o footprint.o
	$(CXX) $(CXXFLAGS) $^ -shared -o $@

malloc.o: cpucores.h
malloc: CPPFLAGS = -DTESTING
malloc: OPTFLAGS = -O0
malloc: malloc.cc makechunk.cc rng.c huge_malloc.cc large_malloc.cc small_malloc.cc bassert.cc footprint.cc | $(wildcard *.h) generated_constants.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) $^ -o $@
objsizes: malloc_internal.h
generated_constants.h: objsizes
	./$< > $@

ALL_SOURCES_INCLUDING_OBJSIZES = $(patsubst %.cc, %, $(wildcard *.cc)) $(patsubst %.c, %, $(wildcard *.c))
ALL_LIB_SOURCES = $(filter-out objsizes, $(ALL_SOURCES_INCLUDING_OBJSIZES))
objsizes $(patsubst %, %.o, $(ALL_LIB_SOURCES)): bassert.h
# Must name generated_constants.h specifically, since wildcard won't find it after a clean.
$(patsubst %, %.o, $(ALL_LIB_SOURCES)): $(wildcard *.h) generated_constants.h

foo:
	echo needs generated_constants.h: $(patsubst %, %.o, $(ALL_LIB_SOURCES))

check: malloc tests_check
	./malloc
tests_check: libsupermalloc.so
	cd tests;$(MAKE) check
clean:
	rm -f t malloc *.o *.so generated_constants.h objsizes *.gcda
	cd tests;$(MAKE) clean
