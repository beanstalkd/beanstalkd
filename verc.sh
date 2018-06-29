#!/bin/sh

echo_n()
{
  if [ "`echo -n`" = "-n" ]; then
    echo "$@""\c"
  else
    echo -n "$@"
  fi
}

echo_n 'const char version[] = "'
./vers.sh
echo '";'
