#
# Makefile
# Adrian Perez, 2010-07-28 00:44
#

CPPFLAGS += -D_DEBUG
CFLAGS   ?= -O0 -g -Wall -W
DESTDIR  ?=
prefix   ?= /usr/local


all: dmon dlog dsyslog

dmon: dmon.o util.o iolib.o
dlog: dlog.o util.o iolib.o
dsyslog: dsyslog.o util.o iolib.o

man: dmon.8 dlog.8 dsyslog.8

%.8: %.rst
	rst2man $< $@

clean:
	$(RM) dmon.o dlog.o dsyslog.o util.o iolib.o
	$(RM) dmon dlog dsyslog


install:
	install -d $(DESTDIR)$(prefix)/share/man/man8
	install -m 644 dmon.8 dlog.8 dsyslog.8 \
		$(DESTDIR)$(prefix)/share/man/man8
	install -d $(DESTDIR)$(prefix)/bin
	install -m 755 dmon dlog dsyslog \
		$(DESTDIR)$(prefix)/bin


.PHONY: man install

# vim:ft=make
#

