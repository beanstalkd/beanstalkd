[![Build Status](https://github.com/beanstalkd/beanstalkd/actions/workflows/build-latest.yaml/badge.svg)](https://github.com/beanstalkd/beanstalkd/actions/workflows/build-latest.yaml)
[![codecov](https://codecov.io/gh/beanstalkd/beanstalkd/branch/master/graph/badge.svg)](https://codecov.io/gh/beanstalkd/beanstalkd)

# beanstalkd

Simple and fast general purpose work queue.

https://beanstalkd.github.io/

See [doc/protocol.txt](https://github.com/beanstalkd/beanstalkd/blob/master/doc/protocol.txt)
for details of the network protocol.

Please note that this project is released with a Contributor
Code of Conduct. By participating in this project you agree
to abide by its terms. See CodeOfConduct.txt for details.

## Quick Start

    $ make
    $ ./beanstalkd


also try,

    $ ./beanstalkd -h
    $ ./beanstalkd -VVV
    $ make CFLAGS=-O2
    $ make CC=clang
    $ make check
    $ make install
    $ make install PREFIX=/usr

Requires Linux (2.6.17 or later), Mac OS X, FreeBSD, or Illumos.

Currently beanstalkd is tested with GCC and clang, but it should work
with any compiler that supports C99.

Uses ronn to generate the manual.
See http://github.com/rtomayko/ronn.


## Subdirectories

- `adm`	- files useful for system administrators
- `ct`	- testing tool; vendored from https://github.com/kr/ct
- `doc`	- documentation
- `pkg`	- scripts to make releases


## Tests

Unit tests are in test*.c. See https://github.com/kr/ct for
information on how to write them.

