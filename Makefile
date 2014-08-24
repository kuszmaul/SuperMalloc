C_CXX_FLAGS = -W -Wall -Werror -O0 -g -pthread -fPIC -mrtm
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS) -std=c11

malloc: CPPFLAGS+=-DTESTING
malloc: malloc.o makehugepage.o rng.o print.o malloc_huge.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(LDLIBS) $^ -o $@
ATOMICALLY_H = atomically.h rng.h
objsizes: malloc_constants.h
generated_constants.h: objsizes
	./$< > $@
makehugepage.o: makehugepage.h $(ATOMICALLY_H) print.h generated_constants.h malloc_constants.h
malloc.o: makehugepage.h generated_constants.h malloc_constants.h
rng.o: rng.h
print.o: print.h
check: malloc
	./malloc
t: generated_constants.h
