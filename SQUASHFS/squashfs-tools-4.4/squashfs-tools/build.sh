#!/bin/bash

export LZMA_LIBDIR=$PWD/../../LIB/LZMA
export LZ4_LIBDIR=$PWD/../../LIB/LZ4
export ZSTD_LIBDIR=$PWD/../../LIB/ZSTD
export LZO_LIBDIR=$PWD/../../LIB/LZO

if [ -e /lib64/libz.a ]; then
    export VTZLIB=/lib64/libz.a
elif [ -e /lib/libz.a ]; then
    export VTZLIB=/lib/libz.a
elif [ -e /usr/lib/libz.a ]; then
    export VTZLIB=/usr/lib/libz.a
fi

rm -f unsquashfs
make clean
make -e unsquashfs

if [ -e unsquashfs ]; then
    strip --strip-all unsquashfs
    echo -e "\n========== SUCCESS ============\n"
else
    echo -e "\n========== FAILED ============\n"
fi

if uname -a | egrep -q 'x86_64|amd64'; then
    name=unsquashfs_64
elif uname -a | egrep -q 'aarch64'; then
    name=unsquashfs_aa64
else
    name=unsquashfs_32
fi

rm -f ../../$name
cp -a unsquashfs ../../$name

