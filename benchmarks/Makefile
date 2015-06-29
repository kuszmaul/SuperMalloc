C_CXX_FLAGS = -W -Wall -Werror -O2 -g -pthread -fPIC
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS)

test: server server-supermalloc server-hoard server-jemalloc server-tbbmalloc profile-supermalloc profile-libc profile-hoard
# test: libfirst.so libinterpose.so
server.o: | random.h
#libfirst.so: firstfitmalloc.o
#	$(CXX) -shared $(CXXFLAGS) $^ -o $@
#libinterpose.so: interpose.o
#	$(CXX) -shared $(CXXFLAGS) $^ -o $@ -ldl

SUPERMALLOC_LFLAGS = -L../release/lib -lsupermalloc -Wl,-rpath,../release/lib -pthread

HOARD_LOCATION = $(CURDIR)/../other-mallocs/Hoard/src
HOARD_LFLAGS = -L$(HOARD_LOCATION) -lhoard -Wl,-rpath,$(HOARD_LOCATION) -pthread

JEMALLOC_LOCATION = $(CURDIR)/../other-mallocs/jemalloc/lib
JEMALLOC_LFLAGS = -L$(JEMALLOC_LOCATION) -ljemalloc -Wl,-rpath,$(JEMALLOC_LOCATION) -pthread

TBB_LOCATION = $(CURDIR)/../other-mallocs/tbb/build/linux_intel64_gcc_cc5.1.1_libc2.21_kernel4.0.4_release
TBB_LFLAGS = -L$(TBB_LOCATION) -ltbbmalloc_proxy -ltbbmalloc  -Wl,-rpath,L$(TBB_LOCATION) -pthread

profile-supermalloc: profile_malloc.o
	$(CXX) $(CXXFLAGS) $< $(SUPERMALLOC_LFLAGS) -o $@
profile-libc: profile_malloc.o
	$(CXX) $(CXXFLAGS) $<                       -o $@
profile-hoard: profile_malloc.o
	$(CXX) $(CXXFLAGS) $< $(HOARD_LFLAGS)       -o $@


server-supermalloc: server.o
	$(CXX) $< $(SUPERMALLOC_LFLAGS) -o $@
server: server.o
	$(CXX) server.o -pthread -o $@
server-hoard: server.o
	$(CXX) server.o $(HOARD_LFLAGS) -o $@
server-jemalloc: server.o
	$(CXX) server.o $(JEMALLOC_LFLAGS) -o $@

server-tbbmalloc: server.o
	$(CXX) server.o $(TBB_LFLAGS) -o $@

#profile_malloc: profile_malloc.o
#	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $<                        -o $@
#profile_malloc-supermalloc: CPPFLAGS += -DSUPERMALLOC -I../src
#profile_malloc-supermalloc: profile_malloc.cpp
#	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< $(SUPERMALLOC_LFLAGS)  -o $@

run: server
	LD_PRELOAD=../release/lib/libsupermalloc.so time ./server
	LD_PRELOAD=$(HOARD_LOCATION)/libhoard.so time ./server
#	LD_PRELOAD=./libfirst.so time ./server

foo1: foo1.cc
	$(CXX) $(CXXFLAGS) $< $(SUPERMALLOC_LFLAGS) -o $@
