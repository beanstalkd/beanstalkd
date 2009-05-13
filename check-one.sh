#!/bin/sh

server=localhost
port=11400
tmpdir="$TMPDIR"
test -z "$tmpdir" && tmpdir=/tmp
tmpf="${tmpdir}/bnch$$"
nc='nc -q 1'

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

./beanstalkd -p $port >/dev/null 2>/dev/null &
bpid=$!

sleep .1
if ! ps $bpid >/dev/null; then
  echo "Could not start beanstalkd for testing, port $port is taken"
  exit 2
fi

# Run the test
fgrep -v "#" $commands | $nc $server $port > "$tmpf"

# Check the output
diff $expected "$tmpf"

