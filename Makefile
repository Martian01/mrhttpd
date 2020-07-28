# Makefile for mrhttpd v2.5.1
#
# Copyright (c) 2007-2011  Martin Rogge <martin_rogge@users.sourceforge.net>

CC = gcc
CFLAGS = -O3
LDFLAGS = 
LIBS = -lpthread

include mrhttpd.conf

default: mrhttpd

config.h: mrhttpd.conf
	sh configure

main.o: main.c mrhttpd.h config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

protocol.o: protocol.c mrhttpd.h config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

io.o: io.c mrhttpd.h config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

mem.o: mem.c mrhttpd.h config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

util.o: util.c mrhttpd.h config.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

OBJ = main.o protocol.o io.o mem.o util.o

mrhttpd: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)
	strip $@

install: mrhttpd
	sh install

semiclean:
	rm -f mrhttpd protocol.o util.o mem.o io.o main.o

clean: semiclean
	rm -f config.h

distclean: clean
