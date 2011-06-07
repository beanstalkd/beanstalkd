#!/bin/sh

VERSION=`mk/vers.sh`
printf "const char version[] = \"$VERSION\";\n"
