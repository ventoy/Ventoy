#!/bin/sh

rm -f vtoyfat_64
rm -f vtoyfat_32

gcc -O2 -D_FILE_OFFSET_BITS=64 vtoyfat_linux.c -Ifat_io_lib/include fat_io_lib/lib/libfat_io_64.a -o vtoyfat_64
gcc -m32 -O2 -D_FILE_OFFSET_BITS=64 vtoyfat_linux.c -Ifat_io_lib/include fat_io_lib/lib/libfat_io_32.a -o vtoyfat_32

if [ -e vtoyfat_64 ] && [ -e vtoyfat_32 ]; then
    echo -e "\n===== success $name =======\n"
    [ -d ../INSTALL/tool/ ] && mv vtoyfat_32 ../INSTALL/tool/ && mv vtoyfat_64 ../INSTALL/tool/
else
    echo -e "\n===== failed =======\n"
    exit 1
fi
