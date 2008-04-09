#!/bin/sh -e

prog="$1"; shift
vers="$1"; shift
file="$1"; shift
pfx="$prog-$vers"

git-archive --format=tar --prefix="$pfx/" "r$vers" | gzip -9 > "$file"
