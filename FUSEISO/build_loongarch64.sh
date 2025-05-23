#!/bin/bash

CUR="$PWD"

LIBFUSE_DIR=$CUR/LIBFUSE

name=vtoy_fuse_iso_la64

#
# use gcc to build for loongarch64
#

export C_INCLUDE_PATH=$LIBFUSE_DIR/include

rm -f $name
gcc -static -O2 -D_FILE_OFFSET_BITS=64  vtoy_fuse_iso.c $LIBFUSE_DIR/lib/libfuse.a  -o  $name

strip --strip-all $name

if [ -e $name ]; then
   echo -e "\n############### SUCCESS $name ##################\n"
else
    echo -e "\n############### FAILED $name ##################\n"
fi

strip --strip-all $name

