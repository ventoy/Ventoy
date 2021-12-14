#!/bin/sh

rm -f vtoycli_64
rm -f vtoycli_32
rm -f vtoycli_aa64
rm -f vtoycli_m64e

SRCS="vtoycli.c vtoyfat.c vtoygpt.c crc32.c partresize.c"

gcc -specs "/usr/local/musl/lib/musl-gcc.specs" -Os -static -D_FILE_OFFSET_BITS=64 $SRCS -Ifat_io_lib/include fat_io_lib/lib/libfat_io_64.a -o vtoycli_64

/opt/diet32/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64 -m32 $SRCS -Ifat_io_lib/include fat_io_lib/lib/libfat_io_32.a -o vtoycli_32


#gcc -O2 -D_FILE_OFFSET_BITS=64 $SRCS -Ifat_io_lib/include fat_io_lib/lib/libfat_io_64.a -o vtoycli_64
#gcc -m32 -O2 -D_FILE_OFFSET_BITS=64 $SRCS -Ifat_io_lib/include fat_io_lib/lib/libfat_io_32.a -o vtoycli_32

aarch64-buildroot-linux-uclibc-gcc -static -O2 -D_FILE_OFFSET_BITS=64 $SRCS -Ifat_io_lib/include fat_io_lib/lib/libfat_io_aa64.a -o vtoycli_aa64
mips64el-linux-musl-gcc -mips64r2 -mabi=64 -static -O2 -D_FILE_OFFSET_BITS=64 $SRCS -Ifat_io_lib/include fat_io_lib/lib/libfat_io_m64e.a -o vtoycli_m64e


if [ -e vtoycli_64 ] && [ -e vtoycli_32 ] && [ -e vtoycli_aa64 ] && [ -e vtoycli_m64e ]; then
    echo -e "\n===== success $name =======\n"
    
    strip --strip-all vtoycli_32
    strip --strip-all vtoycli_64
    aarch64-buildroot-linux-uclibc-strip --strip-all vtoycli_aa64
    mips64el-linux-musl-strip --strip-all vtoycli_m64e
    
    [ -d ../INSTALL/tool/i386/ ] && mv vtoycli_32 ../INSTALL/tool/i386/vtoycli
    [ -d ../INSTALL/tool/x86_64/ ] && mv vtoycli_64 ../INSTALL/tool/x86_64/vtoycli
    [ -d ../INSTALL/tool/aarch64/ ] && mv vtoycli_aa64 ../INSTALL/tool/aarch64/vtoycli
    [ -d ../INSTALL/tool/mips64el/ ] && mv vtoycli_m64e ../INSTALL/tool/mips64el/vtoycli
else
    echo -e "\n===== failed =======\n"
    exit 1
fi
