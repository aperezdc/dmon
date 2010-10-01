#
# Makefile
# Adrian Perez, 2010-07-28 00:44
#


CPPFLAGS  += -D_DEBUG
CFLAGS    ?= -Os -g -Wall -W
DESTDIR   ?=
prefix    ?= /usr/local
MULTICALL ?= 0


MULTICALL := $(strip $(MULTICALL))
ROTLOG    := $(strip $(ROTLOG))

ifneq ($(MULTICALL),0)
  CPPFLAGS += -DMULTICALL
  ifneq ($(ROTLOG),)
    CPPFLAGS += -DMULTICALL_ROTLOG
  endif
else
  ifneq ($(ROTLOG),)
    $(error Bulding rotlog is only supported with MULTICALL=1)
  endif
endif


all: dmon dlog dslog

dmon: dmon.o util.o iolib.o task.o

ifneq ($(MULTICALL),0)
  ifneq ($(ROTLOG),)
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
dslog: dslog.o util.o iolib.o
dlog: dlog.o util.o iolib.o
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
ifneq ($(ROTLOG),)
	$(RM) $(ROTLOG)/rotlog.o
	$(RM) rotlog
endif

install:
	install -d $(DESTDIR)$(prefix)/share/man/man8
	install -m 644 dmon.8 dlog.8 dslog.8 \
		$(DESTDIR)$(prefix)/share/man/man8
	install -d $(DESTDIR)$(prefix)/bin
ifneq ($(MULTICALL),0)
	install -m 755 dmon $(DESTDIR)$(prefix)/bin
	ln -fs dmon $(DESTDIR)$(prefix)/bin/dslog
	ln -fs dmon $(DESTDIR)$(prefix)/bin/dlog
  ifneq ($(ROTLOG),)
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

