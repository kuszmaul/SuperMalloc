default: libtls.so tls
check: default
	LD_PRELOAD=../../release/lib/libsupermalloc_pthread.so  ./tls
CXXFLAGS = -g -W -Wall -Werror -pthread -fPIC
LDLIBS = -ldl
tls: tls.cc
libtls.so: libtls.cc
	gcc $(CCFLAGS) --shared $< -o $@
