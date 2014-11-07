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

TBBMALLOC_DIR = /home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release
$(BLD)/new-malloc-test-tbbmalloc: $(BLD)/new-malloc-test.o
	$(CXX) $< -L$(TBBMALLOC_DIR) -ltbbmalloc_proxy -ltbbmalloc -Wl,-rpath,$(TBBMALLOC_DIR) -pthread -o $@

timings-simple:
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-libc -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-dlmalloc -w 2 -t 10
	/usr/bin/time -f "%M MAXRSS" ./new-malloc-test-hoard -w 2 -t 10

new-malloc-test-1K-x240.data new-malloc-test-1K-lutestring.data: $(BLD)/new-malloc-test $(BLD)/new-malloc-test-dlmalloc $(BLD)/new-malloc-test-hoard $(BLD)/new-malloc-test-jemalloc $(BLD)/new-malloc-test-supermalloc_pthread Makefile $(LIB)/libsupermalloc.so $(LIB)/libsupermalloc_pthread.so
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

new-malloc-test-vK-x240.data new-malloc-test-vK-lutestring.data: $(BLD)/new-malloc-test $(BLD)/new-malloc-test-dlmalloc $(BLD)/new-malloc-test-hoard $(BLD)/new-malloc-test-jemalloc $(BLD)/new-malloc-test-supermalloc_pthread Makefile $(LIB)/libsupermalloc.so $(LIB)/libsupermalloc_pthread.so
	echo threads supermalloc-rtm-nothreadcache supermalloc-rtm supermalloc-lock supermalloc-rtm-nopredo supermalloc-lock-nopredo dlmalloc hoard jemalloc supermalloc_pthread | tee $@
	for trial in $$(seq 1 8); do for thread in $$(seq 1 8); do \
          echo $$thread $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=0 ./$< -s -1 -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -s -1 -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -s -1 -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -s -1 -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -s -1 -w $$thread) \
                        $$(./new-malloc-test-dlmalloc                                                    -s -1 -w $$thread) \
                        $$(./new-malloc-test-hoard                                                       -s -1 -w $$thread) \
                        $$(./new-malloc-test-jemalloc                                                    -s -1 -w $$thread) \
                        $$(./new-malloc-test-supermalloc_pthread                                         -s -1 -w $$thread) \
              | tee -a $@ \
        ;done ;done

new-malloc-test-1K-aggregated.pdf: new-malloc-test-1K-aggregated.data
new-malloc-test-1K-aggregated.data: ../paper/aggregate_error_bars # new-malloc-test-1K-lutestring.data
	./$< < new-malloc-test-1K-lutestring.data > $@

new-malloc-test-vK-aggregated.pdf: new-malloc-test-vK-aggregated.data
new-malloc-test-vK-aggregated.data: ../paper/aggregate_error_bars # new-malloc-test-vK-lutestring.data
	./$< < new-malloc-test-vK-lutestring.data > $@

new-malloc-test-1K-aggregated-oldallocators.pdf: new-malloc-test-1K-aggregated.data

new-malloc-test-1K-tempo.data: NO_RTM=1
new-malloc-test-1K-tempo.data:
	echo threads dlmalloc hoard jemalloc supermalloc_pthread | tee $@
	for trial in $$(seq 1 8); do for thread in $$(seq 1 32); do \
          echo $$thread $$(./new-malloc-test-dlmalloc                                                          -w $$thread) \
                        $$(./new-malloc-test-hoard                                                             -w $$thread) \
                        $$(./new-malloc-test-jemalloc                                                          -w $$thread) \
                        $$(./new-malloc-test-tbbmalloc                                                         -w $$thread) \
                        $$(./new-malloc-test-supermalloc_pthread                                               -w $$thread) \
              | tee -a $@ \
        ;done ;done

new-malloc-test-vK-tempo.data: NO_RTM=1
new-malloc-test-vK-tempo.data:  
	echo threads dlmalloc hoard jemalloc supermalloc_pthread | tee $@
	for trial in $$(seq 1 8); do for thread in $$(seq 1 32); do \
          echo $$thread $$(./new-malloc-test-dlmalloc                                                    -s -1 -w $$thread) \
                        $$(./new-malloc-test-hoard                                                       -s -1 -w $$thread) \
                        $$(./new-malloc-test-jemalloc                                                    -s -1 -w $$thread) \
                        $$(./new-malloc-test-tbbmalloc                                                   -s -1 -w $$thread) \
                        $$(./new-malloc-test-supermalloc_pthread                                         -s -1 -w $$thread) \
              | tee -a $@ \
        ;done ;done

new-malloc-test-vK-tempo-aggregated.pdf: new-malloc-test-vK-tempo-aggregated.data
new-malloc-test-vK-tempo-aggregated.data: ../paper/aggregate_error_bars # new-malloc-test-vK-tempo.data
	./$< < new-malloc-test-vK-tempo.data > $@

new-malloc-test-1K-tempo-aggregated.pdf: new-malloc-test-1K-tempo-aggregated.data
new-malloc-test-1K-tempo-aggregated.data: ../paper/aggregate_error_bars # new-malloc-test-1K-tempo.data
	./$< < new-malloc-test-1K-tempo.data > $@

# Note: supermalloc doesn't compile with gcc 4.8.0, which is what's installed on Tempo.
# I should make it work on tempo, but don't have time right now.
# for now, compile on a machine with more modern gcc
tempo_build: NO_RTM=1
tempo_build: $(BLD)/new-malloc-test-dlmalloc $(BLD)/new-malloc-test-hoard $(BLD)/new-malloc-test-jemalloc $(BLD)/new-malloc-test-supermalloc_pthread Makefile $(LIB)/libsupermalloc_pthread.so
tempo_install:
	rsync -av . tempo.csail.mit.edu:./supermalloc/release
tempo_data: new-malloc-test-1K-tempo.data new-malloc-test-vK-tempo.data
