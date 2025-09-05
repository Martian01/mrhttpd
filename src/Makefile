# Makefile for mrhttpd v2.8.0
#
# Copyright (c) 2007-2021  Martin Rogge <martin_rogge@users.sourceforge.net>

CC = gcc
CFLAGS = -O3
LDFLAGS = 
LIBS = -lpthread

SRC = main.c protocol.c io.c mem.c util.c mrhttpd.h
PRE = main.i protocol.i io.i mem.i util.i
OBJ = main.o protocol.o io.o mem.o util.o

.SUFFIXES:

# exposed targets

default: $(OBJ)
	$(CC) $(LDFLAGS) -o mrhttpd $(OBJ) $(LIBS)

clean:
	rm -f mrhttpd $(OBJ) $(PRE) config.h

pre: $(PRE)

# low-level targets

%.h:
	$(error Catastrophic error: $@ is missing)

%.c:
	$(error Catastrophic error: $@ is missing)

%.o: %.c $(SRC) config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

%.i: %.c $(SRC) config.h
	$(CC) -E -P -o $@ $<

config.h: ../mrhttpd.conf
	( cd .. ; sh configure )
