include mk/inc

CFLAGS=-g -Wall -Werror

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
	srv.o\
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

include mk/cmd
include mk/tst

VERS=$(shell ./vers.sh)
CVERS:=$(shell cat vers.c 2>/dev/null | sed 's/[^"]*"//' | sed 's/".*//')
vers.c:
	printf 'const char version[] = "%s";\n' '$(VERS)' >vers.c

ifneq ($(VERS),$(CVERS))
.PHONY: vers.c
endif

dist: $(TARG)-$(VERS).tar.gz
.PHONY: dist

$(TARG)-$(VERS).tar:
	git archive -o $@ --prefix=$(TARG)-$(VERS)/ v$(VERS)
	tar --delete -f $@ $(TARG)-$(VERS)/vers.sh
	mkdir -p $(TARG)-$(VERS)
	echo 'echo "$(VERS)"' >$(TARG)-$(VERS)/vers.sh
	chmod +x $(TARG)-$(VERS)/vers.sh
	tar --append -f $@ $(TARG)-$(VERS)/vers.sh
	sed 's/@VERSION@/$(VERS)/' <pkg/beanstalkd.spec.in >$(TARG)-$(VERS)/beanstalkd.spec
	tar --append -f $@ $(TARG)-$(VERS)/beanstalkd.spec
	cp NEWS.md $(TARG)-$(VERS)/NEWS.md
	tar --append -f $@ $(TARG)-$(VERS)/NEWS.md
	rm -r $(TARG)-$(VERS)

$(TARG)-$(VERS).tar.gz: $(TARG)-$(VERS).tar
	gzip -f $<
