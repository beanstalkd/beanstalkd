#!/bin/bash

echo "Starting Tests..."
for commands in shell_tests/*.commands; do
  expected=${commands/.commands/.expected}
  echo Testing $(echo $commands | sed -re 's/.*\/(.*)\..*/\1/')
  shell_tests/run_one.sh $commands $expected
  if test $? != 0; then
    echo "!!! TEST FAILED !!!"
    exit 1
  fi
done 
echo -e "\r\n***All tests passed***\r\n"
