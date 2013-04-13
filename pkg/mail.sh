#!/usr/bin/env bash

set -e
set -o pipefail

die() {
	echo >&2 "$@"
	exit 2
}

addr=beanstalk-talk@googlegroups.com
ver=`./vers.sh`
case $ver in *+*) die bad ver $ver ;; esac

(cat <<end; git cat-file -p v$ver:News)|msmtp -t $addr
To: $addr
From: `git config user.name` <`git config user.email`>
Subject: [ANN] beanstalkd $ver
end
