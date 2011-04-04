#!/usr/bin/env bash

if /sbin/ifconfig | egrep -q '^lo[[:alnum:]]*:?[[:space:]]+'; then
    echo "loopback interface is configured, getting on with tests"
else
    echo "loopback interface is NOT configured, won't run tests"
    exit 0
fi

for t in "$@"; do
  echo $t
  $t
  res=$?
  test "$res" = 1 && echo "FAIL: $t"
  test "$res" = 0 || exit 1
done
