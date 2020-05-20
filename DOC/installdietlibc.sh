#!/bin/bash

if ! [ -f ./dietlibc-0.34.tar.xz ]; then
    echo "No dietlibc-0.34.tar.xz found ..."
    exit 1
fi

rm -rf /opt/diet32
rm -rf /opt/diet64

tar -xvf dietlibc-0.34.tar.xz
cd dietlibc-0.34

prefix=/opt/diet64 make -j 4
prefix=/opt/diet64 make install 2>/dev/null

cd ..
rm -rf dietlibc-0.34

tar -xvf dietlibc-0.34.tar.xz
cd dietlibc-0.34

sed "s/MYARCH:=.*/MYARCH=i386/" -i Makefile
sed "s/CC=gcc/CC=gcc -m32/" -i Makefile

prefix=/opt/diet32 make -j 4
prefix=/opt/diet32 make install 2>/dev/null

cd ..
rm -rf dietlibc-0.34

echo ""
echo " ================ success ==============="
echo ""
