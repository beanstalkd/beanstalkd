program := beanstalkd

sources := $(wildcard *.c)

export CFLAGS := -g -pg -Wall -Werror
#export CFLAGS := -O2 -Wall -Werror

export LDFLAGS := -pg -levent

all: $(program)

#ifneq ($(MAKECMDGOALS),clean)
-include $(sources:.c=.d)
#endif

$(program): $(sources:.c=.o)

clean:
	rm -f $(program) *.o *.d core gmon.out

# .DELETE_ON_ERROR:
.PHONY: all clean

# This tells make how to generate dependency files
%.d: %.c
	@$(SHELL) -ec '$(CC) -MM $(CPPFLAGS) $< \
	              | sed '\''s/\($*\)\.o[ :]*/\1.o $@ : /g'\'' > $@; \
				  [ -s $@ ] || rm -f $@'

