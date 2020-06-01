# Changelog
All notable changes to this project will be documented in this file.

## [Unreleased]
### Added
- Support of UNIX domain sockets
- Solaris/illumos support
- the "reserve-job" command 
- draining status to the "stats" command

### Changed
- specify C99 as required compiler
- replaced vendored systemd files with libsystemd
- systemd usage can be controlled with USE_SYSTEMD=yes/no parameter

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


[unreleased]: https://github.com/beanstalkd/beanstalkd/compare/v1.11...HEAD
[1.11]:       https://github.com/beanstalkd/beanstalkd/compare/v1.10...v1.11
[1.10]:       https://github.com/beanstalkd/beanstalkd/compare/v1.9...v1.10
