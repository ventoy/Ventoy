#!/bin/bash

rm -f vtoygpt_64
rm -f vtoygpt_32
rm -f vtoygpt_aa64

/opt/diet64/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64  vtoygpt.c crc32.c -o  vtoygpt_64
/opt/diet32/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64 -m32 vtoygpt.c crc32.c -o  vtoygpt_32

aarch64-buildroot-linux-uclibc-gcc -Os -static -D_FILE_OFFSET_BITS=64  vtoygpt.c crc32.c -o  vtoygpt_aa64

#gcc -D_FILE_OFFSET_BITS=64 -static -Wall vtoygpt.c  -o  vtoytool_64
#gcc -D_FILE_OFFSET_BITS=64  -Wall  -m32  vtoygpt.c  -o  vtoytool_32

if [ -e vtoygpt_64 ] && [ -e vtoygpt_32 ] && [ -e vtoygpt_aa64 ]; then
    echo -e '\n############### SUCCESS ###############\n'
    mv vtoygpt_64 ../INSTALL/tool/x86_64/vtoygpt
    mv vtoygpt_32 ../INSTALL/tool/i386/vtoygpt
    
    aarch64-buildroot-linux-uclibc-strip --strip-all vtoygpt_aa64
    mv vtoygpt_aa64 ../INSTALL/tool/aarch64/vtoygpt
else
    echo -e '\n############### FAILED ################\n'
    exit 1
fi

