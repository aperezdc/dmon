#
# Makefile
# Adrian Perez, 2010-07-28 00:44
#

__verbose := 0

ifeq ($(origin V),command line)
  ifneq ($V,0)
    __verbose := 1
  endif
endif

ifeq ($(__verbose),0)
  define cmd_print
    @printf " %-10s %s\n"
  endef
  silent   := @
  saved_CC := $(CC)
  saved_LD := $(LD)
  saved_AR := $(AR)
  LN        = $(cmd_print) SYMLINK "$@ -> $<" ; ln
  RST2MAN   = $(cmd_print) RST2MAN "$< -> $@"
  STRIP     = $(cmd_print) STRIP   "$@" ; strip
  override CC = $(cmd_print) CC "$@" ; $(saved_CC)
  override LD = $(cmd_print) LD "$@" ; $(saved_LD)
  override AR = $(cmd_print) AR "$@" ; $(saved_AR)
  ifeq ($(findstring clean,$(MAKECMDGOALS)),clean)
    MAKEFLAGS += s
  endif
  ifeq ($(findstring install,$(MAKECMDGOALS)),install)
    MAKEFLAGS += s
  endif
else
  define cmd_print
    @:
  endef
  silent :=
  STRIP   = strip
  LN      = ln
endif

CFLAGS  ?= -Os -g -Wall -W
DESTDIR ?=
prefix  ?= /usr/local

libwheel_PATH := wheel

MULTICALL ?= 1
LIBNOFORK ?= 0

MULTICALL := $(strip $(MULTICALL))
LIBNOFORK := $(strip $(LIBNOFORK))
ROTLOG    := $(strip $(ROTLOG))

ifeq ($(MULTICALL),0)
  CPPFLAGS += -DNO_MULTICALL
endif

all: dmon dlog dslog drlog

libwheel_STDIO   := 0
libwheel_PTHREAD := 0
include $(libwheel_PATH)/Makefile.libwheel

dmon: dmon.o util.o task.o $(libwheel)


ifneq ($(LIBNOFORK),0)
all: libnofork.so
libnofork.so: CFLAGS += -fPIC
libnofork.so: nofork.o -lc
	$(LD) $(LDFLAGS) -shared -o $@ $^
endif


ifneq ($(MULTICALL),0)
dmon: dlog.o dslog.o drlog.o multicall.o
dlog drlog dslog: dmon
	$(LN) -sf $< $@
else
dslog: dslog.o util.o $(libwheel)
drlog: drlog.o util.o $(libwheel)
dlog: dlog.o util.o $(libwheel)
endif

man: dmon.8 dlog.8 dslog.8 drlog.8

%.8: %.rst
	$(RST2MAN) $< $@

ifneq ($(MULTICALL),0)
strip: dmon
else
strip: dmon dslog drlog dlog
endif
	$(STRIP) -x --strip-unneeded $^


clean:
	$(cmd_print) CLEAN
	$(RM) dmon.o dlog.o dslog.o util.o multicall.o task.o drlog.o
	$(RM) dmon dlog dslog drlog
ifneq ($(LIBNOFORK),0)
	$(RM) libnofork.so nofork.o
endif

install: all
	$(cmd_print) INSTALL
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

