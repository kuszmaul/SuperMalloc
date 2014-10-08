# COVERAGE = -fprofile-arcs -ftest-coverage -DCOVERAGE
# STATS = -DENABLE_STATS
# LOGCHECK = -DENABLE_LOG_CHECKING
OPTFLAGS = -O3 -flto

C_CXX_FLAGS = -W -Wall -Werror $(OPTFLAGS) -ggdb -pthread -fPIC -mrtm $(COVERAGE)
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS) -std=c11
CPPFLAGS += $(STATS) $(LOGCHECK)

default: libsupermalloc.so tests_default
.PHONY: default tests_default
tests_default: libsupermalloc.so
	cd tests;$(MAKE) default


malloc.o: cpucores.h
objsizes: OPTFLAGS=-O0
objsizes: COVERAGE=
objsizes: malloc_internal.h
generated_constants.h: objsizes
	./$< > $@

ALL_SOURCES_INCLUDING_OBJSIZES = $(patsubst %.cc, %, $(wildcard *.cc))
ALL_LIB_SOURCES = $(filter-out objsizes, $(ALL_SOURCES_INCLUDING_OBJSIZES))
objsizes $(patsubst %, %.o, $(ALL_LIB_SOURCES)): bassert.h
# Must name generated_constants.h specifically, since wildcard won't find it after a clean.
$(patsubst %, %.o, $(ALL_LIB_SOURCES)): $(wildcard *.h) generated_constants.h

check: tests_check
#check_malloc: malloc
#	SUPERMALLOC_THREADCACHE=1 ./malloc
#	SUPERMALLOC_THREADCACHE=0 ./malloc
.PHONY: tests_check
tests_check: libsupermalloc.so
	cd tests;$(MAKE) check
.PHONY: clean
clean:
	rm -f *.o *.so generated_constants.h objsizes *.gcda *.gcno *.gcov
	cd tests;$(MAKE) clean

TAGS: $(wildcard *.cc) $(wildcard *.h) $(wildcard */*.cc) $(wildcard */*.c)  $(wildcard */*.h)
	etags $^
