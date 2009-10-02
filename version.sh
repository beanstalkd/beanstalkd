#!/bin/sh

# This file will be replaced by "make dist".
git describe | sed 's/^v//' | tr - +
