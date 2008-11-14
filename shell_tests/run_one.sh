#!/bin/bash

SERVER='localhost'
PORT=11300
./beanstalkd 2> /dev/null & 
sleep .1
if ! test -e "/proc/$!"; then
  echo "Could not start beanstalkd for testing"
  exit 1
fi

clean_exit() {
  kill $! 2> /dev/null
  rm .tmp_test
  exit $1
}

fgrep -v "#" $1 | nc $SERVER $PORT > .tmp_test

# diff is "true" if they match
if diff .tmp_test $2; then
  clean_exit 0
fi

clean_exit 1

