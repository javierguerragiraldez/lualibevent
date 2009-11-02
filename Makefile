#
# levent: lua libevent binding
# (c) 2009 Javier Guerra G.
#
 
# Compilation parameters
CC = gcc
CWARNS = -Wall -pedantic \
        -Waggregate-return \
        -Wcast-align \
        -Wmissing-prototypes \
        -Wstrict-prototypes \
        -Wnested-externs \
        -Wpointer-arith \
        -Wshadow \
        -Wwrite-strings


CFLAGS = $(CONFIG) $(CWARNS) -std=gnu99 -g -O2 -I/usr/include/lua5.1 -fPIC


all : levent.so

levent.o : levent.c

levent.so : levent.o
	ld -o levent.so -shared levent.o -levent

clean:
	rm *.o *.so core core.* a.out