#!/bin/sh

rm -f vlnk_64
rm -f vlnk_32
rm -f vlnk_aa64
rm -f vlnk_m64e

SRCS="src/crc32.c src/main_linux.c src/vlnk.c"

gcc -specs "/usr/local/musl/lib/musl-gcc.specs" -Os -static -D_FILE_OFFSET_BITS=64 $SRCS -Isrc -o vlnk_64

/opt/diet32/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64 -m32 -static $SRCS -Isrc -o vlnk_32

aarch64-buildroot-linux-uclibc-gcc -static -O2 -D_FILE_OFFSET_BITS=64 $SRCS -Isrc -o vlnk_aa64
mips64el-linux-musl-gcc -mips64r2 -mabi=64 -static -O2 -D_FILE_OFFSET_BITS=64 $SRCS -Isrc -o vlnk_m64e

if [ -e vlnk_64 ] && [ -e vlnk_32 ] && [ -e vlnk_aa64 ] && [ -e vlnk_m64e ]; then
    echo -e "\n===== success =======\n"
    
    strip --strip-all vlnk_32
    strip --strip-all vlnk_64
    aarch64-buildroot-linux-uclibc-strip --strip-all vlnk_aa64
    mips64el-linux-musl-strip --strip-all vlnk_m64e
    
    [ -d ../INSTALL/tool/i386/ ] && mv vlnk_32 ../INSTALL/tool/i386/vlnk
    [ -d ../INSTALL/tool/x86_64/ ] && mv vlnk_64 ../INSTALL/tool/x86_64/vlnk
    [ -d ../INSTALL/tool/aarch64/ ] && mv vlnk_aa64 ../INSTALL/tool/aarch64/vlnk
    [ -d ../INSTALL/tool/mips64el/ ] && mv vlnk_m64e ../INSTALL/tool/mips64el/vlnk
else
    echo -e "\n===== failed =======\n"
    exit 1
fi
