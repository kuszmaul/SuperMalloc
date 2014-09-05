OPTFLAGS = -O2
CPPFLAGS = -I..
CFLAGS = -ggdb -W -Wall -Werror -std=c99 $(OPTFLAGS)
LDFLAGS = -L.. -Wl,-rpath,..
LDLIBS = -lsupermalloc
TARGETS = \
 aligned_alloc \
 calloc \
 posix_memalign \
 test-malloc_test \
 #end

default: $(TARGETS)
$(TARGETS): | ../supermalloc.h

clean:
	rm -f $(TARGETS)
check: $(patsubst %,check_%,$(TARGETS))
check_%: %
	./$<

test-malloc_test: CFLAGS += -pthread

test-malloc_test.data: test-malloc_test Makefile
	rm -f $@
	echo threads supermalloc jemalloc >> $@
	echo 1 $$(./$< -w 1) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 1) >> $@
	echo 2 $$(./$< -w 2) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 2) >> $@
	echo 3 $$(./$< -w 3) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 3) >> $@
	echo 4 $$(./$< -w 4) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 4) >> $@
	echo 5 $$(./$< -w 5) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 5) >> $@
	echo 6 $$(./$< -w 6) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 6) >> $@
	echo 7 $$(./$< -w 7) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 7) >> $@
	echo 8 $$(./$< -w 8) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 8) >> $@

