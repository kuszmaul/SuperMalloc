# COVERAGE = -fprofile-arcs -ftest-coverage -DCOVERAGE
# STATS = -DENABLE_STATS
# LOGCHECK = -DENABLE_LOG_CHECKING
OPTFLAGS = -O3 -flto

C_CXX_FLAGS = -W -Wall -Werror $(OPTFLAGS) -ggdb -pthread -fPIC -mrtm $(COVERAGE)
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS) -std=c11
CPPFLAGS += $(STATS) $(LOGCHECK)

default: libsupermalloc.so malloc tests_default
.PHONY: default tests_default
tests_default: libsupermalloc.so
	cd tests;$(MAKE) default


# While compiling malloc or any of its .o files, compile with -DTESTING
libsupermalloc.so: malloc.o makechunk.o rng.o huge_malloc.o large_malloc.o small_malloc.o cache.o bassert.o footprint.o stats.o
	$(CXX) $(CXXFLAGS) $^ -shared -o $@

malloc.o: cpucores.h
malloc: CPPFLAGS += -DTESTING
malloc: OPTFLAGS = -O0
malloc: malloc.cc makechunk.cc rng.cc huge_malloc.cc large_malloc.cc small_malloc.cc cache.cc bassert.cc footprint.cc stats.cc $(wildcard *.h) generated_constants.h
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) $(filter-out %.h, $^) -o $@
objsizes: OPTFLAGS=-O0
objsizes: malloc_internal.h
generated_constants.h: objsizes
	./$< > $@

ALL_SOURCES_INCLUDING_OBJSIZES = $(patsubst %.cc, %, $(wildcard *.cc))
ALL_LIB_SOURCES = $(filter-out objsizes, $(ALL_SOURCES_INCLUDING_OBJSIZES))
objsizes $(patsubst %, %.o, $(ALL_LIB_SOURCES)): bassert.h
# Must name generated_constants.h specifically, since wildcard won't find it after a clean.
$(patsubst %, %.o, $(ALL_LIB_SOURCES)): $(wildcard *.h) generated_constants.h

check: check_malloc tests_check
check_malloc: malloc
	SUPERMALLOC_THREADCACHE=1 ./malloc
	SUPERMALLOC_THREADCACHE=0 ./malloc
.PHONY: tests_check
tests_check: libsupermalloc.so
	cd tests;$(MAKE) check
.PHONY: clean
clean:
	rm -f t malloc *.o *.so generated_constants.h objsizes *.gcda
	cd tests;$(MAKE) clean

TAGS: $(wildcard *.cc) $(wildcard *.h) $(wildcard */*.cc) $(wildcard */*.c)  $(wildcard */*.h)
	etags $^
