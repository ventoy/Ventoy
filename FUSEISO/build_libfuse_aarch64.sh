#!/bin/bash

#
#
# Package Dependency:
# gcc automake autoconf gettext gettext-devel libtool unzip
#
#


CUR="$PWD"
LIBFUSE_DIR=$CUR/LIBFUSE

rm -rf libfuse
rm -rf $LIBFUSE_DIR

# please download https://codeload.github.com/libfuse/libfuse/zip/fuse-2.9.9
if [ -e ../ExFAT/mirrors-libfuse-fuse-2.9.9.zip ]; then
    rm -rf libfuse
    unzip ../ExFAT/mirrors-libfuse-fuse-2.9.9.zip
    cd libfuse
elif [ -e ../ExFAT/libfuse-fuse-2.9.9.zip ]; then
    rm -rf libfuse-fuse-2.9.9
    unzip ../ExFAT/libfuse-fuse-2.9.9.zip
    cd libfuse-fuse-2.9.9
else
    echo "Please download mirrors-libfuse-fuse-2.9.9.zip first"
    exit 1
fi


./makeconf.sh

sed '/#define *__u64/d' -i include/fuse_kernel.h
sed '/#define *__s64/d' -i include/fuse_kernel.h

sed  's/__u64/uint64_t/g'    -i include/fuse_kernel.h
sed  's/__s64/int64_t/g'    -i include/fuse_kernel.h

./configure --prefix="$LIBFUSE_DIR" --host=aarch64 CC=aarch64-buildroot-linux-uclibc-gcc


make -j 16
make install
cd ..
rm -rf libfuse
