#!/usr/bin/env bash

. "$SRCDIR/sh-tests/common.functions"

ENOSPC=28
server=localhost
tmpdir="$TMPDIR"
size=1000
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
out2="${tmpdir}/bnch$$.2"
logdir="${tmpdir}/bnch$$.d"
nc="$SRCDIR/sh-tests/netcat.py"

if test "`type -t fiu-run`" = ''
then
  echo ...skipped. '(requires fiu tools from http://blitiri.com.ar/p/libfiu/)'
  exit 0
fi

cleanup() {
    killbeanstalkd
    rm -rf "$logdir" "$out1" "$out2" ${tmpdir}/fiu-ctrl-[0-9]*.{in,out}
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

start_beanstalkd $logdir "-s $size" "fiu-run -x"

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
put 0 0 100 50
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
quit
EOF

diff - "$out1" <<EOF
INSERTED 1
INSERTED 2
INSERTED 3
INSERTED 4
OUT_OF_MEMORY
OUT_OF_MEMORY
EOF
res=$?
test "$res" -eq 0 || exit $res

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
INSERTED 7
INSERTED 8
INSERTED 9
INSERTED 10
EOF
res=$?
test "$res" -eq 0 || exit $res

killbeanstalkd

sleep .1
start_beanstalkd $logdir "-s $size"

$nc $server $port <<EOF > "$out2"
delete 1
delete 2
delete 3
delete 4
delete 7
delete 8
delete 9
delete 10
quit
EOF

diff - "$out2" <<EOF
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
EOF

