#
# Makefile
# Adrian Perez, 2010-07-28 00:44
#


CFLAGS  ?= -Os -g -Wall -W
DESTDIR ?=
prefix  ?= /usr/local

libwheel_PATH := wheel

MULTICALL ?= 0
LIBNOFORK ?= 0

MULTICALL := $(strip $(MULTICALL))
LIBNOFORK := $(strip $(LIBNOFORK))
ROTLOG    := $(strip $(ROTLOG))

ifneq ($(MULTICALL),0)
  CPPFLAGS += -DMULTICALL
endif

all: dmon dlog dslog drlog

libwheel_STDIO   := 0
libwheel_PTHREAD := 0
include $(libwheel_PATH)/Makefile.libwheel

dmon: dmon.o util.o task.o $(libwheel)


ifneq ($(LIBNOFORK),0)
all: libnofork.so
libnofork.so: CFLAGS += -fPIC
libnofork.so: nofork.o
	gcc -shared -o $@ $^
endif


ifneq ($(MULTICALL),0)
dmon: dlog.o dslog.o drlog.o multicall.o
dlog drlog dslog: dmon
	ln -s $< $@
else
dslog: dslog.o util.o $(libwheel)
drlog: drlog.o util.o $(libwheel)
dlog: dlog.o util.o $(libwheel)
endif

man: dmon.8 dlog.8 dslog.8 drlog.8

%.8: %.rst
	rst2man $< $@

ifneq ($(MULTICALL),0)
strip: dmon
else
strip: dmon dslog drlog dlog
endif
	strip -x --strip-unneeded $^


clean:
	$(RM) dmon.o dlog.o dslog.o util.o multicall.o task.o drlog.o
	$(RM) dmon dlog dslog drlog
ifneq ($(LIBNOFORK),0)
	$(RM) libnofork.so nofork.o
endif

install: all
	install -d $(DESTDIR)$(prefix)/share/man/man8
	install -m 644 dmon.8 dlog.8 dslog.8 drlog.8 \
		$(DESTDIR)$(prefix)/share/man/man8
	install -d $(DESTDIR)$(prefix)/bin
ifneq ($(LIBNOFORK),0)
	install -d $(DESTDIR)$(prefix)/lib
	install -m 644 libnofork.so \
		$(DESTDIR)$(prefix)/lib
endif
ifneq ($(MULTICALL),0)
	install -m 755 dmon $(DESTDIR)$(prefix)/bin
	ln -fs dmon $(DESTDIR)$(prefix)/bin/drlog
	ln -fs dmon $(DESTDIR)$(prefix)/bin/dslog
	ln -fs dmon $(DESTDIR)$(prefix)/bin/dlog
else
	install -m 755 dmon dlog dslog drlog \
		$(DESTDIR)$(prefix)/bin
endif

.PHONY: man install strip

# vim:ft=make
#

