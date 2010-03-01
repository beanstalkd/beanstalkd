#!/usr/bin/env bash

. sh-tests/common.functions

server=localhost
tmpdir="$TMPDIR"
size=1024
truncated_size_1=796
truncated_size_2=928
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
out2="${tmpdir}/bnch$$.2"
logdir="${tmpdir}/bnch$$.d"
nc='./sh-tests/netcat.py'

fail() {
    printf 'On line '
    caller
    echo ' ' "$@"
    exit 1
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

start_beanstalkd $logdir "-s $size"

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
quit
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
if ! test "$(fsize "$logdir"/binlog.1)" -eq $truncated_size_1
then
  fail first binlog changed
fi

# Check that the second binlog file is the proper size.
if ! test "$(fsize "$logdir"/binlog.2)" -eq $truncated_size_2
then
  fail second binlog changed
fi

killbeanstalkd

