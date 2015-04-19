OPTFLAGS = -O2
CPPFLAGS = -I../src
CFLAGS = -ggdb -W -Wall -Werror -std=c99 $(OPTFLAGS)
LDFLAGS = -L../release/lib -Wl,-rpath,../release/lib
LDLIBS = -lsupermalloc
TARGETS = \
 aligned_alloc \
 calloc \
 posix_memalign \
 test-malloc_test \
 #end

default: $(TARGETS)
.PHONY: lib
lib:
	cd ../release;make
$(TARGETS): | ../src/supermalloc.h lib

thread-leak: CFLAGS+=-pthread
thread-leak: LDLIBS=
check_thread-leak: thread-leak
	./thread-leak
	LD_PRELOAD=/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/libjemalloc.so ./thread-leak
	LD_PRELOAD=../release/lib/libsupermalloc_pthread.so ./thread-leak

clean:
	rm -f $(TARGETS)
.PHONY: check check_% \
 check-test-malloc_test-w1 \
 check-test-malloc_test-w2 \
 check-test-malloc_test-w1-s4096 \
 check-test-malloc_test-w1-s-1
check: $(patsubst %,check_%,$(TARGETS)) \
 check-test-malloc_test-w1 \
 check-test-malloc_test-w2 \
 check-test-malloc_test-w1-s4096 \
 check-test-malloc_test-w1-s-1
check_%: %
	SUPERMALLOC_THREADCACHE=1 ./$<
	SUPERMALLOC_THREADCACHE=0 ./$<
check-test-malloc_test-w1: test-malloc_test Makefile
	SUPERMALLOC_THREADCACHE=0 ./$< -w1
	SUPERMALLOC_THREADCACHE=1 ./$< -w1
check-test-malloc_test-w2: test-malloc_test Makefile
	SUPERMALLOC_THREADCACHE=0 ./$< -w2
	SUPERMALLOC_THREADCACHE=1 ./$< -w2
check-test-malloc_test-w1-s4096: test-malloc_test Makefile
	SUPERMALLOC_THREADCACHE=0 ./$< -w1 -s 4096
	SUPERMALLOC_THREADCACHE=0 ./$< -w1 -s 4096
check-test-malloc_test-w1-s-1: test-malloc_test Makefile
	SUPERMALLOC_THREADCACHE=0 ./$< -w1 -s -1
	SUPERMALLOC_THREADCACHE=0 ./$< -w1 -s -1

test-malloc_test: CFLAGS += -pthread

.PHONY: data
data: test-malloc_test.data test-malloc_test4K.data

test-malloc_test-1K.data: test-malloc_test Makefile ../libsupermalloc.so
	echo threads supermalloc-rtm-nothreadcache supermalloc-rtm supermalloc-lock supermalloc-rtm-nopredo supermalloc-lock-nopredo jemalloc | tee $@
	for trial in $$(seq 1 8); do for thread in $$(seq 1 8); do \
	  echo $$thread $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=0 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w $$thread) \
              | tee -a $@ \
        ;done ;done

test-malloc_test-nK.data: test-malloc_test Makefile ../libsupermalloc.so
	echo threads supermalloc-rtm-nothreadcache supermalloc-rtm supermalloc-lock supermalloc-rtm-nopredo supermalloc-lock-nopredo jemalloc | tee $@
	for trial in $$(seq 1 8); do for thread in $$(seq 1 8); do \
	  echo $$thread $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=0 ./$< -w $$thread -s -1) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread -s -1) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread -s -1) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread -s -1) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread -s -1) \
                        $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w $$thread) \
              | tee -a $@ \
        ;done ;done


test-malloc_test.data: test-malloc_test Makefile ../libsupermalloc.so
	rm -f $@
	echo threads supermalloc-rtm supermalloc-lock jemalloc | tee $@
	echo 1 $$(SUPERMALLOC_TRANSACTIONS=1 ./$< -w 1) $$(SUPERMALLOC_TRANSACTIONS=0 ./$< -w 1) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 1) | tee -a $@
	echo 2 $$(SUPERMALLOC_TRANSACTIONS=1 ./$< -w 2) $$(SUPERMALLOC_TRANSACTIONS=0 ./$< -w 2) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 2) | tee -a $@
	echo 3 $$(SUPERMALLOC_TRANSACTIONS=1 ./$< -w 3) $$(SUPERMALLOC_TRANSACTIONS=0 ./$< -w 3) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 3) | tee -a $@
	echo 4 $$(SUPERMALLOC_TRANSACTIONS=1 ./$< -w 4) $$(SUPERMALLOC_TRANSACTIONS=0 ./$< -w 4) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 4) | tee -a $@
	echo 5 $$(SUPERMALLOC_TRANSACTIONS=1 ./$< -w 5) $$(SUPERMALLOC_TRANSACTIONS=0 ./$< -w 5) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 5) | tee -a $@
	echo 6 $$(SUPERMALLOC_TRANSACTIONS=1 ./$< -w 6) $$(SUPERMALLOC_TRANSACTIONS=0 ./$< -w 6) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 6) | tee -a $@
	echo 7 $$(SUPERMALLOC_TRANSACTIONS=1 ./$< -w 7) $$(SUPERMALLOC_TRANSACTIONS=0 ./$< -w 7) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 7) | tee -a $@
	echo 8 $$(SUPERMALLOC_TRANSACTIONS=1 ./$< -w 8) $$(SUPERMALLOC_TRANSACTIONS=0 ./$< -w 8) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 8) | tee -a $@



test-malloc_test4K.data: test-malloc_test Makefile ../libsupermalloc.so
	rm -f $@
	echo threads supermalloc jemalloc | tee $@
	echo 1 $$(./$< -w 1 -s 4096) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 1 -s 4096) | tee -a $@
	echo 2 $$(./$< -w 2 -s 4096) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 2 -s 4096) | tee -a $@
	echo 3 $$(./$< -w 3 -s 4096) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 3 -s 4096) | tee -a $@
	echo 4 $$(./$< -w 4 -s 4096) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 4 -s 4096) | tee -a $@
	echo 5 $$(./$< -w 5 -s 4096) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 5 -s 4096) | tee -a $@
	echo 6 $$(./$< -w 6 -s 4096) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 6 -s 4096) | tee -a $@
	echo 7 $$(./$< -w 7 -s 4096) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 7 -s 4096) | tee -a $@
	echo 8 $$(./$< -w 8 -s 4096) $$(LD_PRELOAD=../jemalloc/lib/libjemalloc.so ./$< -w 8 -s 4096) | tee -a $@


# You may need to have done "sudo modprobe msr"
disable_turboboost:
	rdmsr -pi 0x1a0 -f 38:38
