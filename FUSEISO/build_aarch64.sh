#!/bin/bash

CUR="$PWD"

LIBFUSE_DIR=$CUR/LIBFUSE

name=vtoy_fuse_iso_aa64

export C_INCLUDE_PATH=$LIBFUSE_DIR/include


if [ ! -d $LIBFUSE_DIR ]; then
    sh build_libfuse_aarch64.sh
fi

rm -f $name
aarch64-buildroot-linux-uclibc-gcc -static -O2 -D_FILE_OFFSET_BITS=64  vtoy_fuse_iso.c -o $name $LIBFUSE_DIR/lib/libfuse.a

if [ -e $name ]; then
   echo -e "\n############### SUCCESS $name ##################\n"
else
    echo -e "\n############### FAILED $name ##################\n"
fi

aarch64-buildroot-linux-uclibc-strip --strip-all $name

rm -rf $LIBFUSE_DIR
