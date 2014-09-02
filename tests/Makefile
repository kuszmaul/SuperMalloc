CPPFLAGS = -I..
CFLAGS = -g -W -Wall -Werror -std=c99
LDFLAGS = -L.. -Wl,-rpath,..
LDLIBS = -lbmalloc
TARGETS = \
 aligned_alloc \
 calloc \
 #end

default: $(TARGETS)
$(TARGETS): ../bmalloc.h

clean:
	rm aligned_alloc
check: $(patsubst %,check_%,$(TARGETS))
check_%: %
	./$<
