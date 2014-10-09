# CT

**(Relatively) Easy Unit Testing for C**

## How to use

1. Copy subdirectory `ct` into your project.
2. Add some rules to your makefile. See [Makefile][] for an example.
3. Write some tests. See [msg-test.c][] for an example.
   Test function names begin with "cttest".
4. Run `make check`

## Behavior

- The test runner runs each test in a separate process, so
global state from one test will not affect another.
- Each test is run in a new process group; all processes
in the group will be killed after the test finishes. This
means your test can fork without having to worry about
cleaning up its descendants.
- CT participates in GNU make's jobserver protocol. If you
put a `+` in front of the _ctcheck command (as in the sample
makefile) and run make with its `-jN` flag, for example
`make -j16 check`, CT will run tests concurrently (and
hopefully in parallel).
- A scratch directory can be obtained by calling ctdir()
inside the test. This directory will be removed by the test
runner after the test finishes.

## Terminal Output

Running `make -j4 check` in the example supplied looks like this:

```
$ make -j4 check
cc -Werror -Wall -Wformat=2   -c -o msg-test.o msg-test.c
cc -Werror -Wall -Wformat=2   -c -o ct/ct.o ct/ct.c
cc -Werror -Wall -Wformat=2   -c -o msg.o msg.c
ct/gen msg-test.o > ct/_ctcheck.c.part
mv ct/_ctcheck.c.part ct/_ctcheck.c
cc -Werror -Wall -Wformat=2   -c -o ct/_ctcheck.o ct/_ctcheck.c
cc   ct/_ctcheck.o ct/ct.o msg.o msg-test.o   -o ct/_ctcheck
ct/_ctcheck
.......

PASS
```

Remove some of the return statements in msg-test.c to see
what various errors and failures look like.

## Releases

There will be no releases of this tool. Just clone the latest source from git
and copy it into your project. If you want to update, copy the newer source
into your project.

## History

Inspired by [CUT][] 2.1 by Sam Falvo and Billy Tanksley.
Also with ideas from the [Go testing package][gotesting] and [gotest][].
Also stole some benchmark hints from [testingbee][] by Dustin Sallings.

[CUT]: http://falvotech.com/content/cut/
[Makefile]: https://github.com/kr/ct/blob/master/Makefile
[msg-test.c]: https://github.com/kr/ct/blob/master/msg-test.c
[gotesting]: http://golang.org/pkg/testing/
[gotest]: http://golang.org/cmd/go/#Test_packages
[testingbee]: https://github.com/dustin/testingbee
