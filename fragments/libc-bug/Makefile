default: libtls.so tls
check: default
	./tls
CFLAGS = -g -W -Wall -Werror -pthread -fPIC
LDLIBS = -ldl
tls: tls.c
libtls.so: libtls.c
	gcc $(CFLAGS) --shared $< -o $@

tarball: libc-bug.tar.gz
FILES = tls.c libtls.c tls.h Makefile
libc-bug.tar.gz: $(FILES)
	cd ..; tar cfz $(notdir $(CURDIR))/$@ $(patsubst %, $(notdir $(CURDIR))/%, $(FILES))
