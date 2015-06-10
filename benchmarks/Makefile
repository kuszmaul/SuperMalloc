C_CXX_FLAGS = -W -Wall -Werror -O2 -g -pthread -fPIC
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS)

test: server server-supermalloc server-hoard server-jemalloc server-tbbmalloc
# test: libfirst.so libinterpose.so
server.o: | random.h
#libfirst.so: firstfitmalloc.o
#	$(CXX) -shared $(CXXFLAGS) $^ -o $@
#libinterpose.so: interpose.o
#	$(CXX) -shared $(CXXFLAGS) $^ -o $@ -ldl

SUPERMALLOC_LFLAGS = -L../release/lib -lsupermalloc -Wl,-rpath,../release/lib -pthread
server-supermalloc: server.o
	$(CXX) server.o $(SUPERMALLOC_LFLAGS) -o $@
server: server.o
	$(CXX) server.o -pthread -o $@
server-hoard: server.o
	$(CXX) server.o -L/home/bradley/malloc-implementations/Hoard/src/ -lhoard  -Wl,-rpath,/home/bradley/malloc-implementations/Hoard/src/ -pthread -o $@
server-jemalloc: server.o
	$(CXX) server.o -L/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/ -ljemalloc  -Wl,-rpath,/home/bradley/malloc-implementations/jemalloc-3.6.0/lib/ -pthread -o $@

server-tbbmalloc: server.o
	$(CXX) server.o -L/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release -ltbbmalloc_proxy -ltbbmalloc  -Wl,-rpath,/home/bradley/malloc-implementations/tbb43_20140724oss/build/linux_intel64_gcc_cc4.8.3_libc2.18_kernel3.16.3_release -pthread -o $@

profile_malloc: profile_malloc.o
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $<                        -o $@
profile_malloc-supermalloc: CPPFLAGS += -DSUPERMALLOC -I../src
profile_malloc-supermalloc: profile_malloc.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(SUPERMALLOC_LFLAGS)  -o $@

run: server
	LD_PRELOAD=../release/lib/libsupermalloc.so time ./server
	LD_PRELOAD=/home/bradley/malloc-implementations/Hoard/src/libhoard.so time ./server
#	LD_PRELOAD=./libfirst.so time ./server

foo1: foo1.cc
	$(CXX) $(CXXFLAGS) $< $(SUPERMALLOC_LFLAGS) -o $@
