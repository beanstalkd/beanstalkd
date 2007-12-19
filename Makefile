program := beanstalkd
export CFLAGS := $(LDFLAGS) -Wall -Werror
export LDFLAGS := $(LDFLAGS) -levent

sources := $(shell ls *.c | fgrep -v $(program))
objects := $(sources:.c=.o)
tests := $(sources:%=tests/test_%)

all: export CFLAGS := $(CFLAGS) -O2
all: $(program)

debug: export CFLAGS := $(CFLAGS) -g -pg -DDEBUG
debug: export LDFLAGS := $(LDFLAGS) -pg
debug: $(program)

check: export CFLAGS := $(CFLAGS) -g -pg
check: export LDFLAGS := $(LDFLAGS) -pg -levent
check: tests/cutcheck $(objects)
	./tests/cutcheck
	@echo

#ifneq ($(MAKECMDGOALS),clean)
-include $(sources:%.c=.%.d) .$(program).d
#endif

$(program): $(objects) $(program).o

tests/cutcheck.c: $(tests)
	cutgen -o tests/cutcheck.c $(tests)

tests/cutcheck: tests/cutcheck.o $(objects) $(tests:.c=.o)

pkg: check-for-version $(program)-$(VERSION).tar.gz

check-for-version:
	@test "$(VERSION)" || { echo Usage: make pkg 'VERSION=<vers>'; false; }

$(program)-%.tar.gz:
	./pkg.sh $(program) $* $@

clean:
	rm -f $(program) *.o .*.d tests/*.o tests/cutcheck* core gmon.out
	rm -f $(program)-*.tar.gz

# .DELETE_ON_ERROR:
.PHONY: all debug check pkg check-for-version clean

# This tells make how to generate dependency files
.%.d: %.c
	@$(SHELL) -ec '$(CC) -MM $(CPPFLAGS) $< \
	              | sed '\''s/\($*\)\.o[ :]*/\1.o $@ : /g'\'' > $@; \
				  [ -s $@ ] || rm -f $@'

