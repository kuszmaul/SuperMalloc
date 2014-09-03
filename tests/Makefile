OPTFLAGS = -O2
CPPFLAGS = -I..
CFLAGS = -g -W -Wall -Werror -std=c99 $(OPTFLAGS)
LDFLAGS = -L.. -Wl,-rpath,..
LDLIBS = -lbmalloc
TARGETS = \
 aligned_alloc \
 calloc \
 posix_memalign \
 test-malloc_test \
 #end

default: $(TARGETS)
$(TARGETS): | ../bmalloc.h

clean:
	rm aligned_alloc
check: $(patsubst %,check_%,$(TARGETS))
check_%: %
	./$<

test-malloc_test: CFLAGS += -pthread
