#!/bin/bash

one="$(dirname "$0")/check-one.sh"

echo "Starting Tests..."
for commands in "$@"; do
  expected=${commands/.commands/.expected}
  echo Testing $(echo $commands | sed -re 's/.*\/(.*)\..*/\1/')
  $one $commands $expected
  res=$?
  test "$res" = 1 && echo "FAIL: $commands"
  test "$res" = 0 || exit 1
done 
echo -e "\r\n***All tests passed***\r\n"
