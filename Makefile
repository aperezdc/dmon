#
# Makefile
# Adrian Perez, 2010-07-28 00:44
#


CPPFLAGS  += -D_DEBUG
CFLAGS    ?= -Os -g -Wall -W
DESTDIR   ?=
prefix    ?= /usr/local

libwheel_PATH := wheel

MULTICALL ?= 0
LIBNOFORK ?= 0
ROTLOG    ?= 0

MULTICALL := $(strip $(MULTICALL))
LIBNOFORK := $(strip $(LIBNOFORK))
ROTLOG    := $(strip $(ROTLOG))

ifneq ($(MULTICALL),0)
  CPPFLAGS += -DMULTICALL
  ifneq ($(ROTLOG),0)
    CPPFLAGS += -DMULTICALL_ROTLOG
  endif
else
  ifneq ($(ROTLOG),0)
    $(error Bulding rotlog is only supported with MULTICALL=1)
  endif
endif


all: dmon dlog dslog

include $(libwheel_PATH)/Makefile.libwheel

dmon: dmon.o util.o iolib.o task.o $(libwheel)


ifneq ($(LIBNOFORK),0)
all: libnofork.so
libnofork.so: CFLAGS += -fPIC
libnofork.so: nofork.o
	gcc -shared -o $@ $^
endif


ifneq ($(MULTICALL),0)
  ifneq ($(ROTLOG),0)
all: rotlog
dmon: dlog.o dslog.o multicall.o $(ROTLOG)/rotlog.o
$(ROTLOG)/rotlog.o: CPPFLAGS += -Dmain=rotlog_main
dlog dslog rotlog: dmon
  else
dmon: dlog.o dslog.o multicall.o
dlog dslog: dmon
  endif
	ln -s $< $@
else
dslog: dslog.o util.o iolib.o $(libwheel)
dlog: dlog.o util.o iolib.o $(libwheel)
endif

man: dmon.8 dlog.8 dslog.8

%.8: %.rst
	rst2man $< $@

ifneq ($(MULTICALL),0)
strip: dmon
else
strip: dmon dslog dlog
endif
	strip -x --strip-unneeded $^


clean:
	$(RM) dmon.o dlog.o dslog.o util.o iolib.o multicall.o task.o
	$(RM) dmon dlog dslog
ifneq ($(LIBNOFORK),0)
	$(RM) libnofork.so nofork.o
endif
ifneq ($(ROTLOG),0)
	$(RM) $(ROTLOG)/rotlog.o
	$(RM) rotlog
endif

install: all
	install -d $(DESTDIR)$(prefix)/share/man/man8
	install -m 644 dmon.8 dlog.8 dslog.8 \
		$(DESTDIR)$(prefix)/share/man/man8
	install -d $(DESTDIR)$(prefix)/bin
ifneq ($(LIBNOFORK),0)
	install -d $(DESTDIR)$(prefix)/lib
	install -m 644 libnofork.so \
		$(DESTDIR)$(prefix)/lib
endif
ifneq ($(MULTICALL),0)
	install -m 755 dmon $(DESTDIR)$(prefix)/bin
	ln -fs dmon $(DESTDIR)$(prefix)/bin/dslog
	ln -fs dmon $(DESTDIR)$(prefix)/bin/dlog
  ifneq ($(ROTLOG),0)
	ln -fs dmon $(DESTDIR)$(prefix)/bin/rotlog
	install -m 644 $(ROTLOG)/rotlog.8 \
		$(DESTDIR)$(prefix)/share/man/man8
  endif
else
	install -m 755 dmon dlog dslog \
		$(DESTDIR)$(prefix)/bin
endif

.PHONY: man install strip

# vim:ft=make
#

