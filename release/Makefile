ROOT     = ..
BUILD    = release
OPTFLAGS = -O3 -flto
UNITTESTS = # don't include unit tests
LINKMODE = dynamic
include ../Makefile.include

DLMALLOC_DIR = /home/bradley/malloc-implementations/dlmalloc/
$(BLD)/test-malloc_test-dlmalloc: $(BLD)/test-malloc_test.o
	$(CXX) $< -L$(DLMALLOC_DIR) -ldlmalloc -Wl,-rpath,$(DLMALLOC_DIR) -pthread -o $@

HOARD_DIR = /home/bradley/malloc-implementations/Hoard/src
$(BLD)/test-malloc_test-hoard:  $(BLD)/test-malloc_test.o
	$(CXX) $< -L$(HOARD_DIR) -lhoard -Wl,-rpath,$(HOARD_DIR) -pthread -o $@

$(BLD)/new-malloc-test-libc: $(BLD)/new-malloc-test.o
	$(CXX) $< -pthread -o $@

$(BLD)/new-malloc-test-supermalloc_pthread: $(BLD)/new-malloc-test.o $(LIB)/libsupermalloc_pthread.so
	$(CXX) $< -L$(LIB) -lsupermalloc_pthread -Wl,-rpath,$(LIB) -pthread -o $@

$(BLD)/new-malloc-test-dlmalloc: $(BLD)/new-malloc-test.o
	$(CXX) $< -L$(DLMALLOC_DIR) -ldlmalloc -Wl,-rpath,$(DLMALLOC_DIR) -pthread -o $@

$(BLD)/new-malloc-test-hoard:  $(BLD)/new-malloc-test.o
	$(CXX) $< -L$(HOARD_DIR) -lhoard -Wl,-rpath,$(HOARD_DIR) -pthread -o $@

JEMALLOC_DIR = /home/bradley/malloc-implementations/jemalloc-3.6.0/lib
$(BLD)/new-malloc-test-jemalloc:  $(BLD)/new-malloc-test.o
	$(CXX) $< -L$(JEMALLOC_DIR) -ljemalloc -Wl,-rpath,$(JEMALLOC_DIR) -pthread -o $@

timings-simple:
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-libc -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-dlmalloc -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-hoard -w 2 -t 10

new-malloc-test-1Kd-lutestring.data: $(BLD)/new-malloc-test $(BLD)/new-malloc-test-dlmalloc $(BLD)/new-malloc-test-hoard $(BLD)/new-malloc-test-jemalloc $(BLD)/new-malloc-test-supermalloc_pthread Makefile $(LIB)/libsupermalloc.so $(LIB)/libsupermalloc_pthread.so
	echo threads supermalloc-rtm-nothreadcache supermalloc-rtm supermalloc-lock supermalloc-rtm-nopredo supermalloc-lock-nopredo dlmalloc hoard jemalloc supermalloc_pthread | tee $@
	for trial in $$(seq 1 8); do for thread in $$(seq 1 8); do \
          echo $$thread $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=0 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(./new-malloc-test-dlmalloc -w $$thread) \
                        $$(./new-malloc-test-hoard -w $$thread) \
                        $$(./new-malloc-test-jemalloc -w $$thread) \
                        $$(./new-malloc-test-supermalloc_pthread -w $$thread) \
              | tee -a $@ \
        ;done ;done

new-malloc-test-1K-aggregated.pdf: new-malloc-test-1K-aggregated.data
new-malloc-test-1K-aggregated.data: ../paper/aggregate_error_bars new-malloc-test-1K-lutestring.data
	./$< < new-malloc-test-1K-lutestring.data > $@
new-malloc-test-1K-aggregated-oldallocators.pdf: new-malloc-test-1K-aggregated.data
