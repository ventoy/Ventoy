#!/bin/bash

CUR="$PWD"

LIBFUSE_DIR=$CUR/LIBFUSE

if uname -a | egrep -q 'x86_64|amd64'; then
    name=vtoy_fuse_iso_64
else
    name=vtoy_fuse_iso_32
    opt=-lrt
fi

export C_INCLUDE_PATH=$LIBFUSE_DIR/include

rm -f $name
gcc -static -O2 -D_FILE_OFFSET_BITS=64  vtoy_fuse_iso.c -o $name $LIBFUSE_DIR/lib/libfuse.a  -lpthread -ldl $opt

if [ -e $name ]; then
   echo -e "\n############### SUCCESS $name ##################\n"
else
    echo -e "\n############### FAILED $name ##################\n"
fi

strip --strip-all $name

