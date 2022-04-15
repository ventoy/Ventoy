#!/bin/sh

DSTDIR1=../../IMG/cpio_x86/ventoy/busybox
DSTDIR2=../../IMG/cpio_arm64/ventoy/busybox
DSTDIR3=../../IMG/cpio_mips64/ventoy/busybox

rm -f vtchmod32 vtchmod64 vtchmod64_musl vtchmodaa64
rm -f $DSTDIR1/vtchmod32 $DSTDIR1/vtchmod64 $DSTDIR2/vtchmodaa64 $DSTDIR3/vtchmodm64e

/opt/diet32/bin/diet  gcc  -Os -m32  vtchmod.c -o  vtchmod32
/opt/diet64/bin/diet  gcc  -Os       vtchmod.c -o  vtchmod64
aarch64-linux-gcc -Os -static vtchmod.c -o  vtchmodaa64
aarch64-linux-strip --strip-all vtchmodaa64

mips64el-linux-musl-gcc  -mips64r2 -mabi=64 -Os -static vtchmod.c -o  vtchmodm64e
mips64el-linux-musl-strip --strip-all vtchmodm64e


gcc -specs "/usr/local/musl/lib/musl-gcc.specs" -Os -static vtchmod.c -o  vtchmod64_musl
strip --strip-all vtchmod64_musl

chmod 777 vtchmod32
chmod 777 vtchmod64
chmod 777 vtchmodaa64
chmod 777 vtchmod64_musl
chmod 777 vtchmodm64e

cp -a vtchmod32 $DSTDIR1/
cp -a vtchmod64 $DSTDIR1/
cp -a vtchmod64_musl $DSTDIR1/
cp -a vtchmodaa64 $DSTDIR2/
cp -a vtchmodm64e $DSTDIR3/

