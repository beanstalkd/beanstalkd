#!/usr/bin/env bash

. "sh-tests/common.functions"

server=localhost
tmpdir="$TMPDIR"
size=601
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
logdir="${tmpdir}/bnch$$.d"
nc="sh-tests/netcat.py"

cleanup() {
    killbeanstalkd
    rm -rf "$logdir" "$out1"
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

start_beanstalkd $logdir

killbeanstalkd # uses -9, which we need here

start_beanstalkd $logdir

$nc $server $port <<EOF > "$out1"
put 0 0 0 0

quit
EOF

diff - "$out1" <<EOF
INSERTED 1
EOF

