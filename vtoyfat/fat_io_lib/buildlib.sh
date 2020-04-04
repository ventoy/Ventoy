#!/bin/sh

rm -rf include
rm -rf lib

cd release
gcc -O2 -D_FILE_OFFSET_BITS=64 fat*.c -c
ar -rc libfat_io_64.a *.o
rm -f *.o


gcc -m32 -O2 -D_FILE_OFFSET_BITS=64 fat*.c -c
ar -rc libfat_io_32.a *.o
rm -f *.o


cd -

mkdir lib
mkdir include

mv release/*.a lib/
cp -a release/*.h include/


