#!/bin/bash

CUR="$PWD"

rm -rf libfuse
rm -rf LIBFUSE

if [ -e mirrors-libfuse-fuse-2.9.9.zip ]; then
    unzip mirrors-libfuse-fuse-2.9.9.zip
    cd libfuse
else
    unzip libfuse-fuse-2.9.9.zip
    cd libfuse-fuse-2.9.9
fi

./makeconf.sh

./configure --prefix="$CUR/LIBFUSE"
make -j 16
make install
cd ..
rm -rf libfuse
