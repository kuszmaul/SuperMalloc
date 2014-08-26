COVERAGE = -fprofile-arcs -ftest-coverage -DCOVERAGE
C_CXX_FLAGS = -W -Wall -Werror -O0 -g -pthread -fPIC -mrtm $(COVERAGE)
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS) -std=c11
CPPFLAGS = -DTESTING

malloc: malloc.o makechunk.o rng.o huge_malloc.o large_malloc.o small_malloc.o bassert.o footprint.o
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

check: malloc
	./malloc
clean:
	rm -f t malloc *.o generated_constants.h objsizes *.gcda
