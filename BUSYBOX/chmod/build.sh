#!/bin/sh

DSTDIR=../../IMG/cpio/ventoy/busybox

rm -f vtchmod32 vtchmod64
rm -f $DSTDIR/vtchmod32 $DSTDIR/vtchmod64

/opt/diet32/bin/diet  gcc  -Os -m32  vtchmod.c -o  vtchmod32
/opt/diet64/bin/diet  gcc  -Os       vtchmod.c -o  vtchmod64

chmod 777 vtchmod32
chmod 777 vtchmod64

cp -a vtchmod32 $DSTDIR/
cp -a vtchmod64 $DSTDIR/



