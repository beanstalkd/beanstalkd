#!/bin/sh

git describe | tr -d '\n' | sed s/^v// | tr - +
if ! git diff --quiet HEAD
then printf +mod
fi
