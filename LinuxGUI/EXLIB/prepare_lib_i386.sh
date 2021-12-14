#!/bin/bash

if [ ! -d $1/pool ]; then
    echo "$1/pool not exist"
    exit 1
fi

rm -rf i386
mkdir i386
cd i386

cat ../i386libs | while read line; do
    find "$1/pool" -name "*${line}*.deb" | while read deb; do
        echo "extract ${deb##*/} ..."
        dpkg -x $deb .
    done
done

ls -1 ../download/*i386.deb | while read line; do
    echo "extract ${line} ..."
    dpkg -x "$line" .
done

cd ..

