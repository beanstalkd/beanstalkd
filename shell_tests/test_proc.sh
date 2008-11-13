#!/bin/bash

SERVER='localhost'
PORT=11300
./beanstalkd 2> /dev/null & 

bcmd() {
  echo -en "$*"'\r\n' | nc "$SERVER" $PORT
}

clean_exit() {
  kill %1 2> /dev/null
  rm .tmp_test
  exit $1
}

bcmd "put 512 -1 100 0\r\n" > .tmp_test

# diff is "true" if they match
if diff .tmp_test shell_tests/test_proc.expected; then
  clean_exit 0
fi

clean_exit 1

