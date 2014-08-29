C_CXX_FLAGS = -W -Wall -Werror -O2 -g -pthread -fPIC
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS)

test: server server-bmalloc
# test: libfirst.so libinterpose.so
server.o: | random.h
#libfirst.so: firstfitmalloc.o
#	$(CXX) -shared $(CXXFLAGS) $^ -o $@
#libinterpose.so: interpose.o
#	$(CXX) -shared $(CXXFLAGS) $^ -o $@ -ldl

server-bmalloc: server.o
	$(CXX) server.o -L.. -lbmalloc -Wl,-rpath,.. -pthread -o $@
server: server.o
	$(CXX) server.o -pthread -o $@

run: server libfirst.so
	LD_PRELOAD=../libbmalloc.so time ./server
	LD_PRELOAD=/home/bradley/projectlocker/svn/bradleycvs/bmalloc/benchmarks/Hoard/Hoard/src/libhoard.so time ./server
	LD_PRELOAD=./libfirst.so time ./server
