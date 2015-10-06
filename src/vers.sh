#!/bin/sh

if git describe >/dev/null 2>&1
then
    git describe --tags --match=dev* | sed s/^dev// | tr - + | tr -d '\n'
    if ! git diff --quiet HEAD
    then printf +mod
    fi
else
    printf unknown
fi
