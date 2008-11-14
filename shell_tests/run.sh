#!/bin/bash

for commands in shell_tests/*.commands; do
  expected=${commands/.commands/.expected}
  echo "Testing ${commands/.commands/}"
  shell_tests/run_one.sh $commands $expected || exit 1
done 
