#!/bin/sh -e

prog="$1"; shift
vers="$1"; shift
file="$1"; shift

git-archive --format=tar --prefix="$prog-$vers/" "rel-$vers" | gzip > "$file"
