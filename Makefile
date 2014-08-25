# COVERAGE = -fprofile-arcs -ftest-coverage -DCOVERAGE
C_CXX_FLAGS = -W -Wall -Werror -O0 -g -pthread -fPIC -mrtm $(COVERAGE)
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS) -std=c11

malloc: CPPFLAGS+=-DTESTING
malloc: malloc.o makehugepage.o rng.o print.o huge_malloc.o bassert.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) $^ -o $@
ATOMICALLY_H = atomically.h rng.h
objsizes: malloc_internal.h
generated_constants.h: objsizes
	./$< > $@
makehugepage.o: makehugepage.h $(ATOMICALLY_H) print.h generated_constants.h malloc_internal.h
malloc.o: makehugepage.h generated_constants.h malloc_internal.h
rng.o: rng.h
print.o: print.h

$(patsubst %.c, %.o, $(wildcard *.c)): bassert.h

check: malloc
	./malloc
t: generated_constants.h
clean:
	rm -f t malloc *.o
