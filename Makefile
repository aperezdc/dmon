MULTICALL ?= 1
CFLAGS    ?= -Os -g -Wall -W
DESTDIR   ?=
PREFIX    ?= /usr/local
RST2MAN   ?= rst2man
RM        ?= rm -f
LN        ?= ln

CPPFLAGS += -DMULTICALL=$(MULTICALL)

O := deps/cflag/cflag.o deps/clog/clog.o deps/dbuf/dbuf.o \
	conf.o task.o multicall.o util.o $X
D := $(O:.o=.d) dmon.d dlog.d drlog.d dslog.d nofork.d

all: multicall

multicall:
	@$(MAKE) dmon symlinks MULTICALL=1 X='dlog.o dslog.o drlog.o'

standalone:
	@$(MAKE) dmon dlog drlog dslog MULTICALL=0

.PHONY: multicall standalone

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MF $(@:.o=.d) -c -o $@ $<

-include $D

libdmon.a: $O
	$(AR) rcs $@ $?

dmon: dmon.o libdmon.a
	$(CC) $(LDFLAGS) -o $@ dmon.o libdmon.a

dlog: dlog.o libdmon.a
	$(CC) $(LDFLAGS) -o $@ dlog.o libdmon.a

dslog: dslog.o libdmon.a
	$(CC) $(LDFLAGS) -o $@ dslog.o libdmon.a

drlog: drlog.o libdmon.a
	$(CC) $(LDFLAGS) -o $@ drlog.o libdmon.a

nofork: libnofork.so

libnofork.so: nofork.o
	$(CC) $(LDFLAGS) -shared -o $@ nofork.o

.PHONY: nofork

symlinks: dmon
	for i in dlog drlog dslog ; do $(LN) -sf dmon $$i ; done

man: dmon.8 dlog.8 dslog.8 drlog.8

.SUFFIXES: .rst .8

.rst.8:
	$(RST2MAN) $< $@

clean:
	$(RM) dmon dlog dslog drlog libdmon.a dmon.o dlog.o dslog.o drlog.o nofork.o libnofork.so $O

mrproper: clean
	$(RM) $D

install:
	install -d $(DESTDIR)$(PREFIX)/share/man/man8
	install -m 644 dmon.8 dlog.8 dslog.8 drlog.8 \
		$(DESTDIR)$(PREFIX)/share/man/man8
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 dmon dlog dslog drlog \
		$(DESTDIR)$(PREFIX)/bin

.PHONY: man install mrproper nofork
