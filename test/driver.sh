#!/bin/bash

check () {
    if [ $? -eq 0 ]; then
        echo "testing $1: passed"
    else
        echo "testing $1: failed"
        exit 1
    fi
}

tmp=$(mktemp -d /tmp/zecc-test-XXXXXXXXXX)
trap 'rm -rf $tmp' INT TERM HUP EXIT
echo > $tmp/empty.c


# -o
rm -f $tmp/tmp.s
./zecc -o $tmp/tmp.s $tmp/empty.c
[ -f $tmp/tmp.s ]
check "-o"

# -h
./zecc -h 2>&1 | grep -q 'zecc'
check "-h"

# --help
./zecc --help 2>&1 | grep -q 'zecc'
check "--help"