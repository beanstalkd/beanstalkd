#!/usr/bin/env bash

. "$SRCDIR/sh-tests/common.functions"

server=localhost
tmpdir="$TMPDIR"
test -z "$tmpdir" && tmpdir=/tmp
tmpf="${tmpdir}/bnch$$"
nc="$SRCDIR/sh-tests/netcat.py"

commands="$1"; shift
expected="$1"; shift

# Allow generic tests to specify their own behavior completely.
test -x $commands && exec $commands

cleanup() {
    {
        test -z "$bpid" || kill -9 $bpid
        rm -f "$tmpf"
    } >/dev/null 2>&1
}

catch() {
    echo '' Interrupted
    exit 3
}

trap cleanup EXIT
trap catch HUP INT QUIT TERM

if [ ! -x ./beanstalkd ]; then
  echo "Executable ./beanstalkd not found; do you need to compile first?"
  exit 2
fi

start_beanstalkd

# Run the test
fgrep -v "#" $commands | $nc $server $port > "$tmpf"

# Check the output
diff $expected "$tmpf"

