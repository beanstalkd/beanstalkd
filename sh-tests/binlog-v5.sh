#!/usr/bin/env bash

. "sh-tests/common.functions"

server=localhost
tmpdir="$TMPDIR"
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
out2="${tmpdir}/bnch$$.2"
logdir="${tmpdir}/bnch$$.d"
nc="sh-tests/netcat.py"

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

if ! type beanstalkd-1.4.6 >/dev/null
then echo beanstalkd 1.4.6 not found -- skipping binlog-v5. ; exit 0
fi
start_beanstalkd $logdir '' '' beanstalkd-1.4.6

$nc $server $port <<EOF |egrep -v "age|time-left" >"$out1"
use test
put 1 2 3 4
test
put 4 3 2 1
x
stats-job 1
stats-job 2
quit
EOF

diff - "$out1" <<EOF
USING test
INSERTED 1
INSERTED 2
OK 134
---
id: 1
tube: test
state: delayed
pri: 1
delay: 2
ttr: 3
reserves: 0
timeouts: 0
releases: 0
buries: 0
kicks: 0

OK 134
---
id: 2
tube: test
state: delayed
pri: 4
delay: 3
ttr: 2
reserves: 0
timeouts: 0
releases: 0
buries: 0
kicks: 0

EOF
res=$?
test "$res" -eq 0 || exit $res

killbeanstalkd

sleep 1
start_beanstalkd $logdir

$nc $server $port <<EOF |egrep -v "age|time-left" >"$out2"
stats-job 1
stats-job 2
quit
EOF

diff - "$out2" <<EOF
OK 142
---
id: 1
tube: test
state: delayed
pri: 1
delay: 2
ttr: 3
file: 1
reserves: 0
timeouts: 0
releases: 0
buries: 0
kicks: 0

OK 142
---
id: 2
tube: test
state: delayed
pri: 4
delay: 3
ttr: 2
file: 1
reserves: 0
timeouts: 0
releases: 0
buries: 0
kicks: 0

EOF

