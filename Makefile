DESTDIR=
PREFIX=/usr/local
BINDIR=$(DESTDIR)$(PREFIX)/bin
CFLAGS=-Wall -Werror\
	-Wformat=2\
	-g\

LDFLAGS=
OS=$(shell uname|tr A-Z a-z)
INSTALL=install

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
	sd-daemon.o\
	serv.o\
	time.o\
	tube.o\
	util.o\
	vers.o\
	walg.o\

TOFILES=\
	testheap.o\
	testjobs.o\
	testserv.o\
	testutil.o\

HFILES=\
	dat.h\
	sd-daemon.h\

CLEANFILES=\
	vers.c\

.PHONY: all
all: $(TARG)

$(TARG): $(OFILES) $(MOFILE)
	$(LINK.o) -o $@ $^ $(LDLIBS)

.PHONY: install
install: $(BINDIR) $(BINDIR)/$(TARG)

$(BINDIR):
	$(INSTALL) -d $@

$(BINDIR)/%: %
	$(INSTALL) $< $@

CLEANFILES:=$(CLEANFILES) $(TARG)

$(OFILES) $(MOFILE): $(HFILES)

.PHONY: clean
clean:
	rm -f *.o $(CLEANFILES)

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

CLEANFILES:=$(CLEANFILES) ct/_* ct/*.o

ifneq ($(shell ./verc.sh),$(shell cat vers.c 2>/dev/null))
.PHONY: vers.c
endif
vers.c:
	./verc.sh >vers.c

doc/beanstalkd.1 doc/beanstalkd.1.html: doc/beanstalkd.ronn
	ronn $<

freebsd.o: darwin.c
