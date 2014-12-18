default: libtls.so tls
check: default
	LD_PRELOAD=../../release/lib/libsupermalloc_pthread.so  ./tls
CFLAGS = -g -W -Wall -Werror -pthread -fPIC
LDLIBS = -ldl
tls: tls.c
libtls.so: libtls.c
	gcc $(CFLAGS) --shared $< -o $@
