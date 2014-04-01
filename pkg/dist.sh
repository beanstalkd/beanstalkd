#!/usr/bin/env bash

set -e
set -o pipefail

exp() {
	sed s/@VERSION@/$ver/ | sed s/@PARENT@/$prev/
}

clean() {
	rm -f "$GIT_INDEX_FILE"
}

mkobj() {
	git hash-object -w --stdin
}

die() {
	echo >&2 "$@"
	exit 2
}

ver=`./vers.sh`
case $ver in *+*) die bad ver $ver ;; esac
prev=`git describe --abbrev=0 --match=dev* --tags dev$ver^|sed s/^dev//`
test -n "$prev" || die no prev ver
test -f News || die no News

export GIT_INDEX_FILE
GIT_INDEX_FILE=`mktemp -t beanstalkd-dist-index`
trap clean EXIT

git read-tree dev$ver
newsobj=`cat News pkg/newstail.in|exp|mkobj`
versobj=`echo "printf '$ver'"|mkobj`
specobj=`exp <pkg/beanstalkd.spec.in|mkobj`
git update-index --add --cacheinfo 100644 $newsobj News
git update-index --cacheinfo 100755 $versobj vers.sh
git update-index --add --cacheinfo 100644 $specobj beanstalkd.spec
tree=`git write-tree`
commit=`git commit-tree $tree -p dev$ver -m "release $ver"`
git tag -m "beanstalkd version $ver" v$ver $commit

git rev-parse --verify gh-pages >/dev/null
parent=`git rev-parse --verify gh-pages`
git read-tree $parent
postobj=`(exp <pkg/bloghead.in; git cat-file blob v$ver:News)|mkobj`
post=_posts/`date +%Y-%m-%d`-$ver-release-notes.md
git update-index --add --cacheinfo 100644 $postobj $post
tree=`git write-tree`
commit=`git commit-tree $tree -p $parent -m "announce release $ver"`
git update-ref -m "commit: announce release $ver" refs/heads/gh-pages $commit $parent
