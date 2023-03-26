#!/bin/bash

rm -f vtoytool/00/*

/opt/diet64/bin/diet -Os gcc -DVTOY_X86_64 -D_FILE_OFFSET_BITS=64  *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -DUSE_DIET_C  -o  vtoytool_64
/opt/diet32/bin/diet -Os gcc -DVTOY_I386 -D_FILE_OFFSET_BITS=64 -m32  *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -DUSE_DIET_C  -o  vtoytool_32

aarch64-buildroot-linux-uclibc-gcc -Os -static -DVTOY_AA64 -D_FILE_OFFSET_BITS=64  *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL  -o  vtoytool_aa64

mips64el-linux-musl-gcc -mips64r2 -mabi=64 -Os -static -DVTOY_MIPS64 -D_FILE_OFFSET_BITS=64  *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL  -o  vtoytool_m64e

#gcc -D_FILE_OFFSET_BITS=64 -static -Wall -DBUILD_VTOY_TOOL  *.c BabyISO/*.c -IBabyISO  -o  vtoytool_64
#gcc -D_FILE_OFFSET_BITS=64  -Wall -DBUILD_VTOY_TOOL -m32  *.c BabyISO/*.c -IBabyISO  -o  vtoytool_32

if [ -e vtoytool_64 ] && [ -e vtoytool_32 ] && [ -e vtoytool_aa64 ] && [ -e vtoytool_m64e ]; then
    echo -e '\n############### SUCCESS ###############\n'
    
    aarch64-buildroot-linux-uclibc-strip --strip-all vtoytool_aa64
    mips64el-linux-musl-strip --strip-all vtoytool_m64e
    mv vtoytool_m64e vtoytool/00/
    mv vtoytool_aa64 vtoytool/00/
    mv vtoytool_64 vtoytool/00/
    mv vtoytool_32 vtoytool/00/
else
    echo -e '\n############### FAILED ################\n'
    exit 1
fi

