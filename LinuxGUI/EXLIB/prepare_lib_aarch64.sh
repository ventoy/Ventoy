#!/bin/bash

if [ ! -d $1 ]; then
    echo "$1 not exist"
    exit 1
fi

rm -rf aarch64
mkdir aarch64
cd aarch64

cat ../aarch64libs | while read a; do
    ls -1 $1/*$a* | while read rpm; do
        echo "extract ${rpm##*/} ..."
        rpm2cpio $rpm | cpio -idmu --quiet
    done
done

cd ..
