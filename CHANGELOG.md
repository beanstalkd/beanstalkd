# Changelog
All notable changes to this project will be documented in this file.

## [1.12] - 2020-06-04

- add support of UNIX domain sockets
- add support of Solaris/illumos
- add the "reserve-job" command
- add draining status to the "stats" command
- make fsync turned on by default when binlog is used: it's synced every 50ms instead of never
- replace vendored systemd files with libsystemd
- systemd usage can be controlled with USE_SYSTEMD=yes/no
- specify C99 as required compiler

## [1.11] - 2019-06-29

- add automated testing via TravisCI
- add System V init script
- enable code coverage
- misc. fixes and documentation improvements

## [1.10] - 2014-08-05

- fix crash on suspend or other EINTR (#220)
- document touch command’s TTR reset (#188)
- add some basic benchmark tests
- add DESTDIR support to Makefile


[1.12]:       https://github.com/beanstalkd/beanstalkd/compare/v1.11...v1.12
[1.11]:       https://github.com/beanstalkd/beanstalkd/compare/v1.10...v1.11
[1.10]:       https://github.com/beanstalkd/beanstalkd/compare/v1.9...v1.10

