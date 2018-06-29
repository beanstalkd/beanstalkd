#!/bin/sh

echo_n()
{
  if [ "`echo -n`" = "-n" ]; then
    echo "$@""\c"
  else
    echo -n "$@"
  fi
}

if git describe >/dev/null 2>&1
then
    git describe --tags --match='dev*' | sed 's/^dev//' | tr '-' '+' | tr -d '\n'
    if ! git diff --quiet HEAD
	then echo_n +mod
    fi
else
    echo_n unknown
fi
