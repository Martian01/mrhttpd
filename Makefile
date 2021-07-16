# Makefile for mrhttpd v2.7.1
#
# Copyright (c) 2007-2021  Martin Rogge <martin_rogge@users.sourceforge.net>

CC = gcc
CFLAGS = -O3
LDFLAGS = 
LIBS = -lpthread

SRC = main.c protocol.c io.c mem.c util.c mrhttpd.h
PRE = main.i protocol.i io.i mem.i util.i
OBJ = main.o protocol.o io.o mem.o util.o
BIN = mrhttpd

.SUFFIXES:

# exposed targets

default: $(BIN)
	strip $<

install: default
	sh install

semiclean:
	rm -f $(BIN) $(OBJ) $(PRE)

clean: semiclean
	rm -f config.h

distclean: clean

pre: $(PRE)

# low-level targets

mrhttpd.conf:
	$(error Catastrophic error: mrhttpd.conf is missing)

%.h:
	$(error Catastrophic error: $@ is missing)

%.c:
	$(error Catastrophic error: $@ is missing)

%.o: %.c $(SRC) config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

%.i: %.c $(SRC) config.h
	$(CC) -E -P -o $@ $<

config.h: mrhttpd.conf
	sh configure

mrhttpd: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)
