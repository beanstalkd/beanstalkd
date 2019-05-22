FROM ubuntu:16.04

RUN apt-get update && apt-get install -y file git libsystemd-dev libjemalloc-dev make gcc liblz4-dev liblzma-dev libgcrypt20-dev gzip
RUN git clone https://github.com/extendi/beanstalkd && cd beanstalkd && make && mkdir -p INSTALL/usr/bin && cp ./beanstalkd INSTALL/usr/bin
RUN mkdir -p /beanstalkd/INSTALL/usr/lib/x86_64-linux-gnu && cp /usr/lib/x86_64-linux-gnu/libjemalloc.so.1 /beanstalkd/INSTALL/usr/lib/x86_64-linux-gnu
RUN apt-get -y install beanstalkd
RUN mkdir -p /beanstalkd/INSTALL/etc && cp /etc/default/beanstalkd /beanstalkd/INSTALL/etc

RUN cd /beanstalkd/INSTALL && tar czvf /beanstalkd_jemalloc.tar.gz *
