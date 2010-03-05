#!/bin/bash

if /sbin/ifconfig | egrep -q '^lo[[:alnum:]]*:?[[:space:]]+'; then
    echo "loopback interface is configured, getting on with tests"
else
    echo "loopback interface is NOT configured, won't run tests"
    exit 0
fi

one="$(dirname "$0")/check-one.sh"

for commands in "$@"; do
  expected=${commands/.commands/.expected}
  echo $commands
  $one $commands $expected
  res=$?
  test "$res" = 1 && echo "FAIL: $commands"
  test "$res" = 0 || exit 1
done
