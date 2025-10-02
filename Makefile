# Makefile for mrhttpd v2.8.0
#
# Copyright (c) 2007-2021  Martin Rogge <martin_rogge@users.sourceforge.net>

.SUFFIXES:

# exposed targets

default:
	( cd src ; make ; strip mrhttpd )

clean:
	( cd src ; make clean )

install: default
	sh install
