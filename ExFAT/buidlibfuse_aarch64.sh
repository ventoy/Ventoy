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

sed '/#define *__u64/d' -i include/fuse_kernel.h
sed '/#define *__s64/d' -i include/fuse_kernel.h

sed  's/__u64/uint64_t/g'    -i include/fuse_kernel.h
sed  's/__s64/int64_t/g'    -i include/fuse_kernel.h

make -j 16
make install
cd ..
rm -rf libfuse
