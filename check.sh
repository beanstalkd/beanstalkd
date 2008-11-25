#!/bin/bash

one="$(dirname "$0")/check-one.sh"

for commands in "$@"; do
  expected=${commands/.commands/.expected}
  echo $commands
  $one $commands $expected
  res=$?
  test "$res" = 1 && echo "FAIL: $commands"
  test "$res" = 0 || exit 1
done 
