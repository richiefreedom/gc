CFLAGS=-std=gnu99 -Wno-return-local-addr

all: libgc.so testht.c testgc.c
	gcc $(CFLAGS) -O2 testgc.c -o testgc -lpthread -L. -lgc
	gcc $(CFLAGS) -O2 testgc-pt.c -o testgc-pt -lpthread -L. -lgc

libgc.so: hashtable.c hashtable.h gc.c gc.h
	gcc $(CFLAGS) -O2 -fPIC -c gc.c -o gc.o
	gcc $(CFLAGS) -O2 -fPIC -c hashtable.c -o hashtable.o
	gcc -shared hashtable.o gc.o -o libgc.so -lpthread

clean:
	rm -f testht testgc testgc-pt libgc.so *.o
