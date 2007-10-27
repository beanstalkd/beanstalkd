#!/bin/sh -e

git-tag -l | sort | tail -1 | cut -d - -f 2
