#!/usr/bin/env bash

. "sh-tests/common.functions"

ENOSPC=28
server=localhost
tmpdir="$TMPDIR"
size=1000
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
out2="${tmpdir}/bnch$$.2"
logdir="${tmpdir}/bnch$$.d"
nc="sh-tests/netcat.py"

if test "`type -t fiu-run`" = ''
then
  echo ...skipped. '(requires fiu tools from http://blitiri.com.ar/p/libfiu/)'
  exit 0
fi

fail() {
    printf 'On line '
    caller
    echo ' ' "$@"
    exit 1
}

cleanup() {
    killbeanstalkd
    rm -rf "$logdir" "$out1" "$out2" ${tmpdir}/fiu-ctrl-[0-9]*.{in,out}
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

start_beanstalkd $logdir "-s $size" "fiu-run -x"

# Insert enough jobs to create another binlog file
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
quit
EOF

diff - "$out1" <<EOF
INSERTED 1
INSERTED 2
INSERTED 3
INSERTED 4
INSERTED 5
EOF
res=$?
test "$res" -eq 0 || exit $res

# Check that the second binlog file is present
test "$(fsize "$logdir"/binlog.2)" -eq $size || {
    fail Second binlog file is missing
}

# Make beanstalkd think the disk is full now.
fiu-ctrl -e posix/io/oc/open -i $ENOSPC $bpid

# Insert enough jobs to create another binlog file
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
quit
EOF

diff - "$out1" <<EOF
INSERTED 6
INSERTED 7
INSERTED 8
OUT_OF_MEMORY
OUT_OF_MEMORY
EOF
res=$?
test "$res" -eq 0 || exit $res

# Check that the first binlog file is still there
test -e "$logdir"/binlog.1 || fail First binlog file is missing

$nc $server $port <<EOF > "$out1"
delete 1
delete 2
delete 3
delete 4
delete 5
delete 6
delete 7
quit
EOF

diff - "$out1" <<EOF
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
EOF
res=$?
test "$res" -eq 0 || exit $res

# Check that the first binlog file was deleted
test ! -e "$logdir"/binlog.1 || fail First binlog file is still there

# Now make beanstalkd think the disk once again has space.
fiu-ctrl -d posix/io/oc/open $bpid

# Insert enough jobs to create another binlog file
$nc $server $port <<EOF > "$out1"
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
INSERTED 11
INSERTED 12
INSERTED 13
INSERTED 14
EOF
res=$?
test "$res" -eq 0 || exit $res

killbeanstalkd

start_beanstalkd $logdir "-s $size"

$nc $server $port <<EOF > "$out2"
delete 8
delete 11
delete 12
delete 13
delete 14
quit
EOF

diff - "$out2" <<EOF
DELETED
DELETED
DELETED
DELETED
DELETED
EOF

