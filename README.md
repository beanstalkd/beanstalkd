# SETUP
This is beanstalkd, a fast, general-purpose work queue.
See http://kr.github.io/beanstalkd/ for general info.


## QUICK START
```bash
    $ make
    $ ./beanstalkd
```

also try:
```bash
    $ ./beanstalkd -h
    $ ./beanstalkd -VVV
    $ make CFLAGS=-O2
    $ make CC=clang
    $ make check
    $ make install
    $ make install PREFIX=/usr
```

Requires Linux (2.6.17 or later), Mac OS X, or FreeBSD.
See doc/protocol.txt for details of the network protocol.

Uses ronn to generate the manual.
See http://github.com/rtomayko/ronn.

## SUBDIRECTORIES

adm	files useful for system administrators
ct	testing tool; see https://github.com/kr/ct
doc	documentation
pkg	miscellaneous files for packagers


## TESTS

Unit tests are in test*.c. See https://github.com/kr/ct for
information on how to write them.


## DOCKER

```docker build -t beanstalkd_json:v1 .```

## COPYRIGHT
Copyright © 2007-2013 the authors of beanstalkd.
Copyright in contributions to beanstalkd is retained
by the original copyright holder of each contribution.
See file LICENSE for terms of use.
