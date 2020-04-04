#!/bin/bash

rm -f vtoytool/00/*

/opt/diet64/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64  *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -DUSE_DIET_C  -o  vtoytool_64
/opt/diet32/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64 -m32  *.c BabyISO/*.c -IBabyISO -Wall -DBUILD_VTOY_TOOL -DUSE_DIET_C  -o  vtoytool_32

#gcc -D_FILE_OFFSET_BITS=64 -static -Wall -DBUILD_VTOY_TOOL  *.c BabyISO/*.c -IBabyISO  -o  vtoytool_64
#gcc -D_FILE_OFFSET_BITS=64  -Wall -DBUILD_VTOY_TOOL -m32  *.c BabyISO/*.c -IBabyISO  -o  vtoytool_32

if [ -e vtoytool_64 ] && [ -e vtoytool_32 ]; then
    echo -e '\n############### SUCCESS ###############\n'
    
    mv vtoytool_64 vtoytool/00/
    mv vtoytool_32 vtoytool/00/
else
    echo -e '\n############### FAILED ################\n'
    exit 1
fi

