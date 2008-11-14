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

fgrep -v "#" shell_tests/test.commands | nc $SERVER $PORT > .tmp_test

# diff is "true" if they match
if diff .tmp_test shell_tests/test.expected; then
  clean_exit 0
fi

clean_exit 1

