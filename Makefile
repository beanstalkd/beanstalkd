PREFIX?=/usr/local
BINDIR=$(DESTDIR)$(PREFIX)/bin

override CFLAGS+=-Wall -Werror -Wformat=2 -g
override LDFLAGS?=

LDLIBS?=

OS?=$(shell uname | tr 'A-Z' 'a-z')
INSTALL?=install
PKG_CONFIG?=pkg-config

ifeq ($(OS),sunos)
override LDFLAGS += -lxnet -lsocket -lnsl
endif

VERS=$(shell ./vers.sh)
TARG=beanstalkd
MOFILE=main.o
OFILES=\
	$(OS).o\
	conn.o\
	file.o\
	heap.o\
	job.o\
	ms.o\
	net.o\
	primes.o\
	prot.o\
	serv.o\
	time.o\
	tube.o\
	util.o\
	vers.o\
	walg.o\

TOFILES=\
	testheap.o\
	testjobs.o\
	testms.o\
	testserv.o\
	testutil.o\

HFILES=\
	dat.h\

ifeq ($(OS),linux)
   LDLIBS+=-lrt
endif

# systemd support can be configured via USE_SYSTEMD:
#        no: disabled
#       yes: enabled, build fails if libsystemd is not found
# otherwise: enabled if libsystemd is found
ifneq ($(USE_SYSTEMD),no)
ifeq ($(shell $(PKG_CONFIG) --exists libsystemd && echo $$?),0)
	LDLIBS+=$(shell $(PKG_CONFIG) --libs libsystemd)
	CPPFLAGS+=-DHAVE_LIBSYSTEMD
else
ifeq ($(USE_SYSTEMD),yes)
$(error USE_SYSTEMD is set to "$(USE_SYSTEMD)", but $(PKG_CONFIG) cannot find libsystemd)
endif
endif
endif

CLEANFILES=\
	vers.c\
	$(wildcard *.gc*)

.PHONY: all
all: $(TARG)

$(TARG): $(OFILES) $(MOFILE)
	$(LINK.o) -o $@ $^ $(LDLIBS)

.PHONY: install
install: $(BINDIR)/$(TARG)

$(BINDIR)/%: %
	$(INSTALL) -d $(dir $@)
	$(INSTALL) $< $@

CLEANFILES+=$(TARG)

$(OFILES) $(MOFILE): $(HFILES)

CLEANFILES+=$(wildcard *.o)

.PHONY: clean
clean:
	rm -f $(CLEANFILES)

.PHONY: check
check: ct/_ctcheck
	ct/_ctcheck

.PHONY: bench
bench: ct/_ctcheck
	ct/_ctcheck -b

ct/_ctcheck: ct/_ctcheck.o ct/ct.o $(OFILES) $(TOFILES)

ct/_ctcheck.c: $(TOFILES) ct/gen
	ct/gen $(TOFILES) >$@.part
	mv $@.part $@

ct/ct.o ct/_ctcheck.o: ct/ct.h ct/internal.h

$(TOFILES): $(HFILES) ct/ct.h

CLEANFILES+=$(wildcard ct/_* ct/*.o ct/*.gc*)

ifneq ($(shell ./verc.sh),$(shell cat vers.c 2>/dev/null))
.PHONY: vers.c
endif
vers.c:
	./verc.sh >vers.c

doc/beanstalkd.1 doc/beanstalkd.1.html: doc/beanstalkd.ronn
	ronn $<

freebsd.o: darwin.c
