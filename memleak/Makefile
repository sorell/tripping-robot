
CC:=$(CC_PREFIX)cc

lib:
	$(CC) -O2 -Wall -fPIC -c memleak.cpp
	$(CC) -shared -Wl,-soname,libmemleak.so.1 -o libmemleak.so.1 memleak.o -lc
	ln -sf libmemleak.so.1 libmemleak.so

test: lib
	$(MAKE) -C test all

clean:
	rm -f libmemleak.so.1 libmemleak.so memleak.o
	$(MAKE) -C test clean

