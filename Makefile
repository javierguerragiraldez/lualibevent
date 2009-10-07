#
# levent: lua libevent binding
# (c) 2009 Javier Guerra G.
# $Id:  $
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


CFLAGS = $(CONFIG) $(CWARNS) -ansi -g -O2 -I/usr/local/include/lua5


all : levent.so

levent.o : levent.c

levent.so : levent.o
	ld -o levent.so -shared levent.o -levent
