#!/bin/sh

DSTDIR=../../IMG/cpio/ventoy/busybox

rm -f vtchmod32 vtchmod64 vtchmodaa64
rm -f $DSTDIR/vtchmod32 $DSTDIR/vtchmod64 $DSTDIR/vtchmodaa64

/opt/diet32/bin/diet  gcc  -Os -m32  vtchmod.c -o  vtchmod32
/opt/diet64/bin/diet  gcc  -Os       vtchmod.c -o  vtchmod64
aarch64-linux-gcc -Os -static vtchmod.c -o  vtchmodaa64
aarch64-linux-strip --strip-all vtchmodaa64


chmod 777 vtchmod32
chmod 777 vtchmod64
chmod 777 vtchmodaa64

cp -a vtchmod32 $DSTDIR/
cp -a vtchmod64 $DSTDIR/
cp -a vtchmodaa64 $DSTDIR/

