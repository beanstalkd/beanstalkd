#!/bin/bash

SERVER='localhost'
PORT=11400
./beanstalkd -p $PORT >/dev/null 2>/dev/null &
bg=$!

sleep .1
if ! ps $bg >/dev/null; then
  echo "Could not start beanstalkd for testing, port $PORT is taken"
  exit 1
fi

clean_exit() {
  kill -9 $1
  rm .tmp_test
  exit $2
}

fgrep -v "#" $1 | nc -q 1 $SERVER $PORT > .tmp_test

# diff is "false" if they match
if diff $2 .tmp_test; then
  clean_exit $bg 0 2>/dev/null
fi

clean_exit $bg 1 2>/dev/null
