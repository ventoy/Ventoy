#!/bin/bash

if [ -z "$1" ]; then
    echo "please input url"
    exit 1
fi

if [ -n "$2" ]; then
    proxy_opt="-x $2"
fi

rm -rf download
mkdir -p download
cd download

grep pool ../README.txt | while read line; do
    a="$line"
    b=$(basename "$a")
    echo "downloading $b ..."
    curl -s $1/debian/"$a" $proxy_opt -o "$b"

    a=$(echo $line | sed "s/mips64el/i386/g")
    b=$(basename "$a")
    echo "downloading $b ..."
    curl -s $1/debian/"$a" $proxy_opt -o "$b"
done

cd ..
