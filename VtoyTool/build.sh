#!/bin/bash

mkdir -p vtoytool/00

/opt/diet64/bin/diet -Os gcc -DVTOY_X86_64 -D_FILE_OFFSET_BITS=64 *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -DUSE_DIET_C -o vtoytool_64 || true
/opt/diet32/bin/diet -Os gcc -DVTOY_I386 -D_FILE_OFFSET_BITS=64 -m32 *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -DUSE_DIET_C -o vtoytool_32 || true

if command -v aarch64-buildroot-linux-uclibc-gcc >/dev/null 2>&1; then
    aarch64-buildroot-linux-uclibc-gcc -Os -static -DVTOY_AA64 -D_FILE_OFFSET_BITS=64 *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -o vtoytool_aa64
    aarch64-buildroot-linux-uclibc-strip --strip-all vtoytool_aa64
elif command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    aarch64-linux-gnu-gcc -Os -static -DVTOY_AA64 -D_FILE_OFFSET_BITS=64 *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -o vtoytool_aa64
    aarch64-linux-gnu-strip --strip-all vtoytool_aa64
fi

if command -v mips64el-linux-musl-gcc >/dev/null 2>&1; then
    mips64el-linux-musl-gcc -mips64r2 -mabi=64 -Os -static -DVTOY_MIPS64 -D_FILE_OFFSET_BITS=64 *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -o vtoytool_m64e
    mips64el-linux-musl-strip --strip-all vtoytool_m64e
fi

[ -f vtoytool_64 ] && mv -f vtoytool_64 vtoytool/00/
[ -f vtoytool_32 ] && mv -f vtoytool_32 vtoytool/00/
[ -f vtoytool_aa64 ] && mv -f vtoytool_aa64 vtoytool/00/
[ -f vtoytool_m64e ] && mv -f vtoytool_m64e vtoytool/00/

echo -e '\n############### VtoyTool Build Completed ###############\n'
ls -la vtoytool/00/

