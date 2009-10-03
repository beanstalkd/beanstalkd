---
layout: base
title: Download
---

## Source

Beanstalk is distributed under the GNU GPL version 3.

### Release 1.3

 * [`beanstalkd-1.3.tar.gz`](/dist/beanstalkd/beanstalkd-1.3.tar.gz)
    (116,114 bytes) released on 23 March 2009.  
    Read [1.3 release notes](rel/notes-1.3.html).

Also requires [libevent][libevent]
version 1.4.1 or later.

## Packages

### Fedora 9 and Fedora 10

Just type:

    su -c 'yum install beanstalkd'

### CentOS and RHEL

You can find beanstalk in the EPEL testing repository for now. It should be
automatically pushed to stable before 15 Feb 2009.

 * [Information on EPEL](http://fedoraproject.org/wiki/EPEL)

 * [How to use EPEL](http://fedoraproject.org/wiki/EPEL/FAQ#howtouse)

To install immediately on CentOS or RHEL:

    su -c 'rpm -Uvh http://download.fedora.redhat.com/pub/epel/5/i386/epel-release-5-3.noarch.rpm'
    su -c 'yum install beanstalkd --enablerepo=epel-testing'

You can drop the `--enablerepo` bits when the next stable epel is
pushed.

### Gentoo

Just type:

    sudo emerge beanstalkd

## Client Libraries

See the [client library](client.html) page to find a library for your favorite
language.

## Git

Browse the source online at <http://github.com/kr/beanstalkd>.

To get your own copy and start hacking, just run:

    git clone git://github.com/kr/beanstalkd.git

You can also find beanstalkd source at <http://repo.or.cz/w/beanstalkd.git>.

[libevent]: http://monkey.org/~provos/libevent/
