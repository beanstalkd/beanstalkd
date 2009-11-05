#!/usr/bin/env bash

server=localhost
port=11400
tmpdir="$TMPDIR"
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
out2="${tmpdir}/bnch$$.2"
logdir="${tmpdir}/bnch$$.d"
nc='nc -q 1'
nc -q 1 2>&1 | grep -q "illegal option" && nc='nc -w 1' # workaround for older netcat

killbeanstalkd() {
    {
        test -z "$bpid" || kill -9 $bpid
        /bin/true # Somehow this gets rid of an unnessary shell message.
    } >/dev/null 2>&1
    bpid=
}

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

mkdir -p $logdir

./beanstalkd -p $port -b "$logdir" >/dev/null 2>/dev/null &
bpid=$!

sleep .1
if ! ps -p $bpid >/dev/null; then
  echo "Could not start beanstalkd for testing (possibly port $port is taken)"
  exit 2
fi

$nc $server $port <<EOF > "$out1"
put 0 0 100 0

EOF

diff - "$out1" <<EOF
INSERTED 1
EOF
res=$?
test "$res" -eq 0 || exit $res

killbeanstalkd

sleep .1
./beanstalkd -p $port -b "$logdir" >/dev/null 2>/dev/null &
bpid=$!

sleep .1
if ! ps -p $bpid >/dev/null; then
  echo "Could not start beanstalkd for testing (possibly port $port is taken)"
  exit 2
fi

$nc $server $port <<EOF > "$out2"
delete 1
EOF

diff - "$out2" <<EOF
DELETED
EOF

