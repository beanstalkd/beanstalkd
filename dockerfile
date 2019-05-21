FROM ubuntu:16.04

RUN apt-get update && apt-get install -y git libjemalloc-dev make gcc
RUN git clone https://github.com/extendi/beanstalkd && cd beanstalkd && make
