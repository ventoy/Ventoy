#!/bin/bash

if [ ! -d $1/pool ]; then
    echo "$1/pool not exist"
    exit 1
fi

rm -rf mips64el
mkdir mips64el
cd mips64el

cat ../mips64ellibs | while read line; do
    find "$1/pool" -name "*${line}*.deb" | while read deb; do
        echo "extract ${deb##*/} ..."
        dpkg -x $deb .
    done
done

dpkg -x ../libgtk-3-dev_3.22.11-1_mips64el.deb .

cd ..



