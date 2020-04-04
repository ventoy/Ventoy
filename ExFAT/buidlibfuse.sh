#!/bin/bash

CUR="$PWD"

rm -rf libfuse
rm -rf LIBFUSE

unzip mirrors-libfuse-fuse-2.9.9.zip


cd libfuse
./makeconf.sh

./configure --prefix="$CUR/LIBFUSE"
make -j 16
make install
cd ..
rm -rf libfuse
