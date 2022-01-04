ARG BASE=alpine
FROM alpine as builder
RUN apk add --no-cache build-base git
COPY . /tmp/beanstalkd
RUN cd /tmp/beanstalkd && make

################################
ARG BASE
FROM ${BASE}

COPY --from=builder /tmp/beanstalkd/beanstalkd /usr/bin/
EXPOSE 11300
ENTRYPOINT ["/usr/bin/beanstalkd"]
