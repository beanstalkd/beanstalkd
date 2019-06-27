# beanstalkd

This is beanstalkd, a fast, general-purpose work queue.
See https://beanstalkd.github.io/ for general info.

Please note that this project is released with a Contributor
Code of Conduct. By participating in this project you agree
to abide by its terms. See CodeOfConduct.txt for details.

[![Build Status](https://travis-ci.org/beanstalkd/beanstalkd.svg?branch=master)](https://travis-ci.org/beanstalkd/beanstalkd)

## Quick Stat

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

Requires Linux (2.6.17 or later), Mac OS X, or FreeBSD.
See `doc/protocol.txt` for details of the network protocol.

Uses ronn to generate the manual.
See http://github.com/rtomayko/ronn.


## Subdirectories

- `adm`	- files useful for system administrators
- `ct`	- testing tool; see https://github.com/kr/ct
- `doc`	- documentation
- `pkg`	- miscellaneous files for packagers


## Tests

Unit tests are in test*.c. See https://github.com/kr/ct for
information on how to write them.


Copyright © 2007-2013 the authors of beanstalkd.
Copyright in contributions to beanstalkd is retained
by the original copyright holder of each contribution.
See file LICENSE for terms of use.
