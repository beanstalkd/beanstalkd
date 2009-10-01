#!/usr/bin/env bash

server=localhost
port=11400
tmpdir="$TMPDIR"
size=1024
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
out2="${tmpdir}/bnch$$.2"
logdir="${tmpdir}/bnch$$.d"
nc='nc -q 1'
nc -q 1 2>&1 | grep -q option && nc='nc -w 1' # workaround for older netcat

fail() {
    printf 'On line '
    caller
    echo ' ' "$@"
    exit 1
}

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

# Yuck.
fsize() {
    ls -l -- "$@" | awk '{ print $5 }'
}

trap cleanup EXIT
trap catch HUP INT QUIT TERM

if [ ! -x ./beanstalkd ]; then
  echo "Executable ./beanstalkd not found; do you need to compile first?"
  exit 2
fi

mkdir -p $logdir

./beanstalkd -p $port -b "$logdir" -s $size >/dev/null 2>/dev/null &
bpid=$!

sleep .1
if ! ps -p $bpid >/dev/null; then
  echo "Could not start beanstalkd for testing (possibly port $port is taken)"
  exit 2
fi

# Check that the first binlog file is the proper size.
test "$(fsize "$logdir"/binlog.1)" -eq $size || fail first binlog wrong size

# Insert enough jobs to create a second binlog file
$nc $server $port <<EOF > "$out1"
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
EOF

diff - "$out1" <<EOF
INSERTED 1
INSERTED 2
INSERTED 3
INSERTED 4
INSERTED 5
INSERTED 6
INSERTED 7
INSERTED 8
INSERTED 9
INSERTED 10
INSERTED 11
INSERTED 12
INSERTED 13
INSERTED 14
INSERTED 15
INSERTED 16
INSERTED 17
INSERTED 18
INSERTED 19
INSERTED 20
EOF
res=$?
test "$res" -eq 0 || exit $res

# Check that the first binlog file is still the proper size.
test "$(fsize "$logdir"/binlog.1)" -eq $size || fail first binlog changed

# Check that the second binlog file is the proper size.
test "$(fsize "$logdir"/binlog.2)" -eq $size || fail second binlog changed

killbeanstalkd

