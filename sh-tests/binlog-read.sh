#!/usr/bin/env bash

. sh-tests/common.functions

server=localhost
tmpdir="$TMPDIR"
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
out2="${tmpdir}/bnch$$.2"
logdir="${tmpdir}/bnch$$.d"
nc='./sh-tests/netcat.py'

cleanup() {
    killbeanstalkd
    rm -rf "$logdir" "$out1" "$out2"
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

$nc $server $port <<EOF > "$out1"
use test
put 0 0 120 4
test
put 0 0 120 4
tes1
watch test
reserve
release 1 1 1
reserve
delete 2
quit
EOF

diff - "$out1" <<EOF
USING test
INSERTED 1
INSERTED 2
WATCHING 2
RESERVED 1 4
test
RELEASED
RESERVED 2 4
tes1
DELETED
EOF
res=$?
test "$res" -eq 0 || exit $res

killbeanstalkd

sleep 1
start_beanstalkd $logdir

$nc $server $port <<EOF > "$out2"
watch test
reserve
delete 1
delete 2
quit
EOF

diff - "$out2" <<EOF
WATCHING 2
RESERVED 1 4
test
DELETED
NOT_FOUND
EOF

