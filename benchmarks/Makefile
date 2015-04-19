C_CXX_FLAGS = -W -Wall -Werror -O2 -g -pthread -fPIC
CXXFLAGS = $(C_CXX_FLAGS) -std=c++11
CFLAGS = $(C_CXX_FLAGS)

test: server server-supermalloc
# test: libfirst.so libinterpose.so
server.o: | random.h
#libfirst.so: firstfitmalloc.o
#	$(CXX) -shared $(CXXFLAGS) $^ -o $@
#libinterpose.so: interpose.o
#	$(CXX) -shared $(CXXFLAGS) $^ -o $@ -ldl

server-supermalloc: server.o
	$(CXX) server.o -L../release/lib -lsupermalloc -Wl,-rpath,../release/lib -pthread -o $@
server: server.o
	$(CXX) server.o -pthread -o $@

run: server
	LD_PRELOAD=../release/lib/libsupermalloc.so time ./server
	LD_PRELOAD=/home/bradley/malloc-implementations/Hoard/src/libhoard.so time ./server
#	LD_PRELOAD=./libfirst.so time ./server
