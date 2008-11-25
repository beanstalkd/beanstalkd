#!/bin/bash

one="$(dirname "$0")/run_one.sh"

echo "Starting Tests..."
for commands in "$@"; do
  expected=${commands/.commands/.expected}
  echo Testing $(echo $commands | sed -re 's/.*\/(.*)\..*/\1/')
  $one $commands $expected
  if test $? != 0; then
    echo "!!! TEST FAILED !!!"
    exit 1
  fi
done 
echo -e "\r\n***All tests passed***\r\n"
