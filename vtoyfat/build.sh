#!/bin/sh

rm -f vtoyfat_64
rm -f vtoyfat_32
rm -f vtoyfat_aa64
rm -f vtoyfat_m64e


gcc -O2 -D_FILE_OFFSET_BITS=64 vtoyfat_linux.c -Ifat_io_lib/include fat_io_lib/lib/libfat_io_64.a -o vtoyfat_64
gcc -m32 -O2 -D_FILE_OFFSET_BITS=64 vtoyfat_linux.c -Ifat_io_lib/include fat_io_lib/lib/libfat_io_32.a -o vtoyfat_32
aarch64-buildroot-linux-uclibc-gcc -static -O2 -D_FILE_OFFSET_BITS=64 vtoyfat_linux.c -Ifat_io_lib/include fat_io_lib/lib/libfat_io_aa64.a -o vtoyfat_aa64
mips64el-linux-musl-gcc -mips64r2 -mabi=64 -static -O2 -D_FILE_OFFSET_BITS=64 vtoyfat_linux.c -Ifat_io_lib/include fat_io_lib/lib/libfat_io_m64e.a -o vtoyfat_m64e


if [ -e vtoyfat_64 ] && [ -e vtoyfat_32 ] && [ -e vtoyfat_aa64 ] && [ -e vtoyfat_m64e ]; then
    echo -e "\n===== success $name =======\n"
    
    strip --strip-all vtoyfat_32
    strip --strip-all vtoyfat_64
    aarch64-buildroot-linux-uclibc-strip --strip-all vtoyfat_aa64
    mips64el-linux-musl-strip --strip-all vtoyfat_m64e
    
    [ -d ../INSTALL/tool/i386/ ] && mv vtoyfat_32 ../INSTALL/tool/i386/vtoyfat
    [ -d ../INSTALL/tool/x86_64/ ] && mv vtoyfat_64 ../INSTALL/tool/x86_64/vtoyfat
    [ -d ../INSTALL/tool/aarch64/ ] && mv vtoyfat_aa64 ../INSTALL/tool/aarch64/vtoyfat
    [ -d ../INSTALL/tool/mips64el/ ] && mv vtoyfat_m64e ../INSTALL/tool/mips64el/vtoyfat
else
    echo -e "\n===== failed =======\n"
    exit 1
fi
