#!/bin/bash

rm -f vtoytool/00/*

/opt/diet64/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64  vtoygpt.c crc32.c -o  vtoygpt_64
/opt/diet32/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64 -m32 vtoygpt.c crc32.c -o  vtoygpt_32

#gcc -D_FILE_OFFSET_BITS=64 -static -Wall vtoygpt.c  -o  vtoytool_64
#gcc -D_FILE_OFFSET_BITS=64  -Wall  -m32  vtoygpt.c  -o  vtoytool_32

if [ -e vtoygpt_64 ] && [ -e vtoygpt_32 ]; then
    echo -e '\n############### SUCCESS ###############\n'
    mv vtoygpt_64 ../INSTALL/tool/
    mv vtoygpt_32 ../INSTALL/tool/
else
    echo -e '\n############### FAILED ################\n'
    exit 1
fi

