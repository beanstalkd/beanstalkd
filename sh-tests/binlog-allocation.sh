#!/usr/bin/env bash

. "$SRCDIR/sh-tests/common.functions"

server=localhost
tmpdir="$TMPDIR"
size=601
test -z "$tmpdir" && tmpdir=/tmp
out1="${tmpdir}/bnch$$.1"
logdir="${tmpdir}/bnch$$.d"
nc="$SRCDIR/sh-tests/netcat.py"

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

start_beanstalkd $logdir "-s $size"

$nc $server $port <<EOF > "$out1"
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
put 0 0 120 22
job payload xxxxxxxxxx
delete 1
delete 2
delete 3
delete 4
delete 5
delete 6
delete 7
delete 8
delete 9
delete 10
delete 11
delete 12
delete 13
delete 14
delete 15
delete 16
delete 17
delete 18
delete 19
delete 20
delete 21
delete 22
delete 23
delete 24
delete 25
delete 26
delete 27
delete 28
delete 29
delete 30
delete 31
delete 32
delete 33
delete 34
delete 35
delete 36
delete 37
delete 38
delete 39
delete 40
delete 41
delete 42
delete 43
delete 44
delete 45
delete 46
delete 47
delete 48
delete 49
delete 50
delete 51
delete 52
delete 53
delete 54
delete 55
delete 56
delete 57
delete 58
delete 59
delete 60
delete 61
delete 62
delete 63
delete 64
delete 65
delete 66
delete 67
delete 68
delete 69
delete 70
delete 71
delete 72
delete 73
delete 74
delete 75
delete 76
delete 77
delete 78
delete 79
delete 80
delete 81
delete 82
delete 83
delete 84
delete 85
delete 86
delete 87
delete 88
delete 89
delete 90
delete 91
delete 92
delete 93
delete 94
delete 95
delete 96
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
INSERTED 21
INSERTED 22
INSERTED 23
INSERTED 24
INSERTED 25
INSERTED 26
INSERTED 27
INSERTED 28
INSERTED 29
INSERTED 30
INSERTED 31
INSERTED 32
INSERTED 33
INSERTED 34
INSERTED 35
INSERTED 36
INSERTED 37
INSERTED 38
INSERTED 39
INSERTED 40
INSERTED 41
INSERTED 42
INSERTED 43
INSERTED 44
INSERTED 45
INSERTED 46
INSERTED 47
INSERTED 48
INSERTED 49
INSERTED 50
INSERTED 51
INSERTED 52
INSERTED 53
INSERTED 54
INSERTED 55
INSERTED 56
INSERTED 57
INSERTED 58
INSERTED 59
INSERTED 60
INSERTED 61
INSERTED 62
INSERTED 63
INSERTED 64
INSERTED 65
INSERTED 66
INSERTED 67
INSERTED 68
INSERTED 69
INSERTED 70
INSERTED 71
INSERTED 72
INSERTED 73
INSERTED 74
INSERTED 75
INSERTED 76
INSERTED 77
INSERTED 78
INSERTED 79
INSERTED 80
INSERTED 81
INSERTED 82
INSERTED 83
INSERTED 84
INSERTED 85
INSERTED 86
INSERTED 87
INSERTED 88
INSERTED 89
INSERTED 90
INSERTED 91
INSERTED 92
INSERTED 93
INSERTED 94
INSERTED 95
INSERTED 96
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
DELETED
EOF

