MULTICALL = 1
CFLAGS    = -Os -g -Wall -W
PREFIX    = /usr/local
RST2MAN   = rst2man
RM        = rm -f

APPLETS   = dlog drlog dslog

O = deps/cflag/cflag.o deps/clog/clog.o deps/dbuf/dbuf.o \
	conf.o task.o multicall.o util.o
D = $(O:.o=.d) dmon.d nofork.d setunbuf.d $(APPLETS:=.d)

all: all-multicall-$(MULTICALL)

all-multicall-1:
	@$(MAKE) programs A='$(APPLETS)'

all-multicall-0:
	@$(MAKE) programs P='$(APPLETS)'

programs: dmon $(APPLETS)

.PHONY: all-multicall-0 all-multicall-1 programs

.c.o: CFLAGS := $(CFLAGS)
setunbuf.o: CFLAGS := $(CFLAGS) -fPIC

.c.o:
	$(CC) -DMULTICALL=$(MULTICALL) $(CFLAGS) -MMD -MF $(@:.o=.d) -c -o $@ $<

.SUFFIXES: .c .o

-include $D

libdmon.a: $O $(A:=.o)
	$(AR) rcs $@ $?

dmon $P: dmon.o $(P:=.o) libdmon.a
	$(CC) $(LDFLAGS) -o $@ $@.o libdmon.a $(LDLIBS)

nofork: libnofork.so

libnofork.so: nofork.o
	$(CC) $(LDFLAGS) -shared -o $@ nofork.o $(LDLIBS)

setunbuf: libsetunbuf.so

libsetunbuf.so: setunbuf.o
	$(CC) $(LDFLAGS) -shared -o $@ setunbuf.o $(LDLIBS)

.PHONY: nofork setunbuf

$(A:=-symlink): $A

$A: dmon
	ln -sf dmon $@

.PHONY: $(A:=-symlink)

man: dmon.8 dlog.8 dslog.8 drlog.8

.rst.8:
	$(RST2MAN) $< $@

.PHONY: man
.SUFFIXES: .rst .8

clean:
	$(RM) dmon dlog dslog drlog libdmon.a dmon.o dlog.o dslog.o drlog.o nofork.o libnofork.so setunbuf.o libsetunbuf.so $O

mrproper: clean
	$(RM) $D

.PHONY: clean mrproper

install-all: install-all-multicall-$(MULTICALL)

install-all-multicall-1: install-common
	ln -sf dmon $(DESTDIR)$(PREFIX)/bin/dlog
	ln -sf dmon $(DESTDIR)$(PREFIX)/bin/drlog
	ln -sf dmon $(DESTDIR)$(PREFIX)/bin/dslog

install-all-multicall-0: install-common
	install -m 755 $(APPLETS) $(DESTDIR)$(PREFIX)/bin

install-common:
	install -d $(DESTDIR)$(PREFIX)/share/man/man8
	install -m 644 dmon.8 dlog.8 dslog.8 drlog.8 \
		$(DESTDIR)$(PREFIX)/share/man/man8
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 dmon $(DESTDIR)$(PREFIX)/bin

.PHONY: install-common install-all-multicall-0 install-all-multicall-0

install: install-multicall-$(MULTICALL)

install-multicall-1: all-multicall-1
	@$(MAKE) install-all A='$(APPLETS)'

install-multicall-0: all-multicall-0
	@$(MAKE) install-all P='$(APPLETS)'

.PHONY: install install-multicall-0 install-multicall-1

dist:
	@$(MAKE) dist-files VERSION=$$(jq -r .version package.json)

dist-files: \
	dmon-$(VERSION).tar.xz \
	dmon-$(VERSION).tar.xz.asc \
	dmon-$(VERSION).tar.xz.sha512

dmon-$(VERSION).tar.xz:
	git archive --format=tar --prefix=dmon-$(VERSION)/ v$(VERSION) | xz -9c > $@

dmon-$(VERSION).tar.xz.asc: dmon-$(VERSION).tar.xz
	$(RM) $@
	gpg --armor --detach-sign --output=$@ dmon-$(VERSION).tar.xz

dmon-$(VERSION).tar.xz.sha512: dmon-$(VERSION).tar.xz
	sha512sum --tag dmon-$(VERSION).tar.xz > $@

.PHONY: dist dist-files
