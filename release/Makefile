ROOT     = ..
BUILD    = release
OPTFLAGS = -O3 -flto
UNITTESTS = # don't include unit tests
LINKMODE = dynamic
include ../Makefile.include

DLMALLOC_DIR = /home/bradley/malloc-implementations/dlmalloc/
DLMALLOC_FLAGS = -L$(DLMALLOC_DIR) -ldlmalloc -Wl,-rpath,$(DLMALLOC_DIR) -pthread

HOARD_DIR = /home/bradley/malloc-implementations/Hoard/src
HOARD_FLAGS = -L$(HOARD_DIR) -lhoard -Wl,-rpath,$(HOARD_DIR) -pthread

JEMALLOC_DIR = /home/bradley/malloc-implementations/jemalloc-3.6.0/lib
JEMALLOC_FLAGS = -L$(JEMALLOC_DIR) -ljemalloc -Wl,-rpath,$(JEMALLOC_DIR) -pthread

TBBMALLOC_DIR = /home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release
TBBMALLOC_FLAGS = -L$(TBBMALLOC_DIR) -ltbbmalloc_proxy -ltbbmalloc -Wl,-rpath,$(TBBMALLOC_DIR) -pthread 

TCMALLOC_DIR = /home/bradley/malloc-implementations/gperftools-2.4/.libs
TCMALLOC_FLAGS = -L$(TCMALLOC_DIR) -ltcmalloc_minimal -Wl,-rpath,$(TCMALLOC_DIR) -pthread

$(BLD)/test-malloc_test-dlmalloc: $(BLD)/test-malloc_test.o
	$(CXX) $< $(DLMALLOC_FLAGS) -o $@
$(BLD)/test-malloc_test-hoard:  $(BLD)/test-malloc_test.o
	$(CXX) $< $(HOARD_FLAGS) -o $@

$(BLD)/malloc-test-fixed-work-libc: $(BLD)/malloc-test-fixed-work.o
	$(CXX) $< -pthread -o $@
$(BLD)/malloc-test-fixed-work-dlmalloc: $(BLD)/malloc-test-fixed-work.o
	$(CXX) $< $(DLMALLOC_FLAGS) -o $@
$(BLD)/malloc-test-fixed-work-hoard: $(BLD)/malloc-test-fixed-work.o
	$(CXX) $< $(HOARD_FLAGS) -o $@
$(BLD)/malloc-test-fixed-work-jemalloc:  $(BLD)/malloc-test-fixed-work.o
	$(CXX) $< $(JEMALLOC_FLAGS) -o $@
$(BLD)/malloc-test-fixed-work-tbbmalloc: $(BLD)/malloc-test-fixed-work.o
	$(CXX) $< $(TBBMALLOC_FLAGS) -o $@

$(BLD)/new-malloc-test-libc: $(BLD)/new-malloc-test.o
	$(CXX) $< -pthread -o $@
$(BLD)/new-malloc-test-dlmalloc: $(BLD)/new-malloc-test.o
	$(CXX) $< $(DLMALLOC_FLAGS) -o $@
$(BLD)/new-malloc-test-hoard:  $(BLD)/new-malloc-test.o
	$(CXX) $< $(HOARD_FLAGS) -o $@
$(BLD)/new-malloc-test-jemalloc:  $(BLD)/new-malloc-test.o
	$(CXX) $< $(JEMALLOC_FLAGS) -o $@
$(BLD)/new-malloc-test-tbbmalloc: $(BLD)/new-malloc-test.o
	$(CXX) $< $(TBBMALLOC_FLAGS) -o $@
$(BLD)/new-malloc-test-tcmalloc: $(BLD)/new-malloc-test.o
	$(CXX) $< $(TCMALLOC_FLAGS) -o $@

$(BLD)/bradley-malloc-test-libc: $(BLD)/bradley-malloc-test.o
	$(CXX) $< -pthread -o $@
$(BLD)/bradley-malloc-test-dlmalloc: $(BLD)/bradley-malloc-test.o
	$(CXX) $< $(DLMALLOC_FLAGS) -o $@
$(BLD)/bradley-malloc-test-hoard: $(BLD)/bradley-malloc-test.o
	$(CXX) $< $(HOARD_FLAGS) -o $@
$(BLD)/bradley-malloc-test-jemalloc: $(BLD)/bradley-malloc-test.o
	$(CXX) $< $(JEMALLOC_FLAGS) -o $@
$(BLD)/bradley-malloc-test-tbbmalloc: $(BLD)/bradley-malloc-test.o
	$(CXX) $< $(TBBMALLOC_FLAGS) -o $@
$(BLD)/bradley-malloc-test-tcmalloc: $(BLD)/bradley-malloc-test.o
	$(CXX) $< $(TCMALLOC_FLAGS) -o $@

$(BLD)/new-malloc-test-supermalloc_pthread: $(BLD)/new-malloc-test.o $(LIB)/libsupermalloc_pthread.so
	$(CXX) $< -L$(LIB) -lsupermalloc_pthread -Wl,-rpath,$(LIB) -pthread -o $@


TBBMALLOCNOTSX_DIR = /home/bradley/malloc-implementations/tbb_no_tsx/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release
$(BLD)/new-malloc-test-tbbmallocnotsx: $(BLD)/new-malloc-test.o
	$(CXX) $< -L$(TBBMALLOCNOTSX_DIR) -ltbbmalloc_proxy -ltbbmalloc -Wl,-rpath,$(TBBMALLOCNOTSX_DIR) -pthread -o $@

timings-simple: $(BLD)/bradley-malloc-test $(BLD)/bradley-malloc-test-libc $(BLD)/bradley-malloc-test-dlmalloc $(BLD)/bradley-malloc-test-hoard $(BLD)/bradley-malloc-test-jemalloc $(BLD)/bradley-malloc-test-tbbmalloc $(BLD)/bradley-malloc-test-tcmalloc
	/usr/bin/time ./bradley-malloc-test           -w 2
	/usr/bin/time ./bradley-malloc-test-libc      -w 2
	/usr/bin/time ./bradley-malloc-test-dlmalloc  -w 2
	/usr/bin/time ./bradley-malloc-test-hoard     -w 2
	/usr/bin/time ./bradley-malloc-test-jemalloc  -w 2
	/usr/bin/time ./bradley-malloc-test-tbbmalloc -w 2
	/usr/bin/time ./bradley-malloc-test-tcmalloc  -w 2

timings-simple-old: $(BLD)/new-malloc-test $(BLD)/new-malloc-test-libc $(BLD)/new-malloc-test-dlmalloc $(BLD)/new-malloc-test-hoard $(BLD)/new-malloc-test-jemalloc $(BLD)/new-malloc-test-tbbmalloc $(BLD)/new-malloc-test-tcmalloc
	/usr/bin/time -f "%U %S %E %M MAXRSS" ./new-malloc-test -w 2 -t 10
	/usr/bin/time -f "%U %S %E %M MAXRSS" ./new-malloc-test-libc -w 2 -t 10
	/usr/bin/time -f "%U %S %E %M MAXRSS" ./new-malloc-test-dlmalloc -w 2 -t 10
	/usr/bin/time -f "%U %S %E %M MAXRSS" ./new-malloc-test-hoard -w 2 -t 10
	/usr/bin/time -f "%U %S %E %M MAXRSS" ./new-malloc-test-jemalloc -w 2 -t 10
	/usr/bin/time -f "%U %S %E %M MAXRSS" ./new-malloc-test-tbbmalloc -w 2 -t 10
	/usr/bin/time -f "%U %S %E %M MAXRSS" ./new-malloc-test-tcmalloc -w 2 -t 10

timings-fixed-work: $(BLD)/malloc-test-fixed-work $(BLD)/malloc-test-fixed-work-libc $(BLD)/malloc-test-fixed-work-dlmalloc $(BLD)/malloc-test-fixed-work-hoard $(BLD)/malloc-test-fixed-work-jemalloc $(BLD)/malloc-test-fixed-work-tbbmalloc
	/usr/bin/time ./malloc-test-fixed-work           -w 2
	/usr/bin/time ./malloc-test-fixed-work-libc      -w 2
	/usr/bin/time ./malloc-test-fixed-work-dlmalloc  -w 2
	/usr/bin/time ./malloc-test-fixed-work-hoard     -w 2
	/usr/bin/time ./malloc-test-fixed-work-jemalloc  -w 2
	/usr/bin/time ./malloc-test-fixed-work-tbbmalloc -w 2



x240-timings: new-malloc-test-1K-x240.data new-malloc-test-vK-x240.data

just-test-supermalloc: $(BLD)/new-malloc-test
	for trial in $$(seq 1 8); do for thread in $$(seq 8 8); do \
          echo $$thread \
          $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=0 ./$< -w $$thread) \
          $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
          $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
          $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
          $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
        ;done;done


new-malloc-test-1K-x240.data new-malloc-test-1K-lutestring.data: $(BLD)/new-malloc-test \
 $(BLD)/new-malloc-test-dlmalloc \
 $(BLD)/new-malloc-test-hoard \
 $(BLD)/new-malloc-test-jemalloc \
 $(BLD)/new-malloc-test-tbbmalloc \
 $(BLD)/new-malloc-test-supermalloc_pthread \
 $(BLD)/new-malloc-test-tbbmallocnotsx \
 Makefile $(LIB)/libsupermalloc.so $(LIB)/libsupermalloc_pthread.so
	echo threads supermalloc-rtm-nothreadcache supermalloc-rtm supermalloc-lock supermalloc-rtm-nopredo supermalloc-lock-nopredo dlmalloc hoard jemalloc tbbmalloc supermalloc_pthread tbbmallocnotsx | tee $@
	for trial in $$(seq 1 8); do for thread in $$(seq 1 8); do \
          echo $$thread $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=0 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -w $$thread) \
                        $$(./new-malloc-test-dlmalloc -w $$thread) \
                        $$(./new-malloc-test-hoard -w $$thread) \
                        $$(./new-malloc-test-jemalloc -w $$thread) \
                        $$(./new-malloc-test-tbbmalloc -w $$thread) \
                        $$(./new-malloc-test-supermalloc_pthread -w $$thread) \
                        $$(./new-malloc-test-tbbmallocnotsx -w $$thread) \
              | tee -a $@ \
        ;done ;done

new-malloc-test-vK-x240.data new-malloc-test-vK-lutestring.data: $(BLD)/new-malloc-test $(BLD)/new-malloc-test-dlmalloc $(BLD)/new-malloc-test-hoard $(BLD)/new-malloc-test-jemalloc $(BLD)/new-malloc-test-supermalloc_pthread Makefile $(LIB)/libsupermalloc.so $(LIB)/libsupermalloc_pthread.so
	echo threads supermalloc-rtm-nothreadcache supermalloc-rtm supermalloc-lock supermalloc-rtm-nopredo supermalloc-lock-nopredo dlmalloc hoard jemalloc tbbmalloc supermalloc_pthread tbbmalloc-notsx | tee $@
	for trial in $$(seq 1 8); do for thread in $$(seq 1 8); do \
          echo $$thread $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=0 ./$< -s -1 -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -s -1 -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=1 SUPERMALLOC_THREADCACHE=1 ./$< -s -1 -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=1 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -s -1 -w $$thread) \
                        $$(SUPERMALLOC_TRANSACTIONS=0 SUPERMALLOC_PREDO=0 SUPERMALLOC_THREADCACHE=1 ./$< -s -1 -w $$thread) \
                        $$(./new-malloc-test-dlmalloc                                                    -s -1 -w $$thread) \
                        $$(./new-malloc-test-hoard                                                       -s -1 -w $$thread) \
                        $$(./new-malloc-test-jemalloc                                                    -s -1 -w $$thread) \
                        $$(./new-malloc-test-tbbmalloc                                                   -s -1 -w $$thread) \
                        $$(./new-malloc-test-supermalloc_pthread                                         -s -1 -w $$thread) \
                        $$(./new-malloc-test-tbbmalloc-notsx                                             -s -1 -w $$thread) \
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
	echo threads dlmalloc hoard jemalloc tbbmalloc supermalloc_pthread | tee $@
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
	echo threads dlmalloc hoard jemalloc tbbmalloc supermalloc_pthread | tee $@
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

new-malloc-test-vK-lutestring-aggregated.pdf: new-malloc-test-vK-lutestring-aggregated.data
new-malloc-test-vK-lutestring-aggregated.data: ../paper/aggregate_error_bars # new-malloc-test-vK-lutestring.data
	./$< < new-malloc-test-vK-lutestring.data > $@

new-malloc-test-1K-lutestring-aggregated.pdf: new-malloc-test-1K-lutestring-aggregated.data
new-malloc-test-1K-lutestring-aggregated.data: ../paper/aggregate_error_bars # new-malloc-test-1K-lutestring.data
	./$< < new-malloc-test-1K-lutestring.data > $@

# Note: supermalloc doesn't compile with gcc 4.8.0, which is what's installed on Tempo.
# I should make it work on tempo, but don't have time right now.
# for now, compile on a machine with more modern gcc
tempo_build: NO_RTM=1
tempo_build: $(BLD)/new-malloc-test-dlmalloc $(BLD)/new-malloc-test-hoard $(BLD)/new-malloc-test-jemalloc $(BLD)/new-malloc-test-supermalloc_pthread Makefile $(LIB)/libsupermalloc_pthread.so
tempo_install:
	rsync -av . tempo.csail.mit.edu:./supermalloc/release
tempo_data: new-malloc-test-1K-tempo.data new-malloc-test-vK-tempo.data

$(BLD)/t-test1: $(BLD)/../tests/t-test1/t-test1.c
	$(CXX) $(CXXFLAGS) $< -o $@

SUPERMALLOC_LIB_DIR = ../release/lib
SUPERMALLOC_LDLIBS = -lsupermalloc -ldl
SUPERMALLOC_LDFLAGS = -Wl,-rpath,$(SUPERMALLOC_LIB_DIR) -L$(SUPERMALLOC_LIB_DIR)
NO_BUILTINS =  -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
$(BLD)/t-test1-supermalloc: $(BLD)/../tests/t-test1/t-test1.c $(BLD)/../tests/t-test1/t-test.h 
	$(CXX) $(NO_BUILTINS) $(CXXFLAGS) $< $(SUPERMALLOC_LDFLAGS) $(SUPERMALLOC_LDLIBS)   -o $@ 

# $(BLD)/t-test1: $(BLD)/../tests/t-test1/t-test1.c $(BLD)/../tests/t-test1/t-test.h 
#	$(CXX) $(NO_BUILTINS) $(CXXFLAGS) $< $(TCMALLOC_FLAGS)   -o $@ 
