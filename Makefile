PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
CFLAGS=-Wall -Werror\
	-Wformat=2\

LDFLAGS=
OS=$(shell uname -s | tr A-Z a-z)
INSTALL=install
TAR=tar

VERS=$(shell ./vers.sh)
TARG=beanstalkd
MOFILE=main.o
OFILES=\
	conn.o\
	file.o\
	heap.o\
	job.o\
	ms.o\
	net.o\
	port-$(OS).o\
	primes.o\
	prot.o\
	sd-daemon.o\
	sock-$(OS).o\
	serv.o\
	time.o\
	tube.o\
	util.o\
	vers.o\
	walg.o\

TOFILES=\
	heap-test.o\
	integ-test.o\
	job-test.o\
	util-test.o\

HFILES=\
	dat.h\
	sd-daemon.h\

CLEANFILES=\
	vers.c\
	$(TARG)-*.tar.gz\

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

.PHONY: dist
dist: $(TARG)-$(VERS).tar.gz

$(TARG)-$(VERS).tar:
	git archive -o $@ --prefix=$(TARG)-$(VERS)/ v$(VERS)
	mkdir -p $(TARG)-$(VERS)/mk
	echo 'printf "$(VERS)"' >$(TARG)-$(VERS)/vers.sh
	chmod +x $(TARG)-$(VERS)/vers.sh
	$(TAR) --append -f $@ $(TARG)-$(VERS)/vers.sh
	sed 's/@VERSION@/$(VERS)/' <pkg/beanstalkd.spec.in >$(TARG)-$(VERS)/beanstalkd.spec
	$(TAR) --append -f $@ $(TARG)-$(VERS)/beanstalkd.spec
	cp NEWS.md $(TARG)-$(VERS)/NEWS.md
	$(TAR) --append -f $@ $(TARG)-$(VERS)/NEWS.md
	rm -r $(TARG)-$(VERS)

$(TARG)-$(VERS).tar.gz: $(TARG)-$(VERS).tar
	gzip -f $<

doc/beanstalkd.1 doc/beanstalkd.1.html: doc/beanstalkd.ronn
	ronn $<

sock-darwin.o sock-freebsd.o: sock-bsd.c
