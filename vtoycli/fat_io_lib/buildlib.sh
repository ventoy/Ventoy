#!/bin/sh

rm -rf include
rm -rf lib

cd release
#/opt/diet64/bin/diet -Os gcc -O2 -D_FILE_OFFSET_BITS=64 fat*.c -c
gcc -specs "/usr/local/musl/lib/musl-gcc.specs" -O2 -D_FILE_OFFSET_BITS=64 fat*.c -c
ar -rc libfat_io_64.a *.o
rm -f *.o


gcc -m32 -O2 -D_FILE_OFFSET_BITS=64 fat*.c -c
ar -rc libfat_io_32.a *.o
rm -f *.o


aarch64-linux-gnu-gcc -O2 -D_FILE_OFFSET_BITS=64 fat*.c -c
ar -rc libfat_io_aa64.a *.o
rm -f *.o


mips64el-linux-musl-gcc -mips64r2 -mabi=64 -O2 -D_FILE_OFFSET_BITS=64 fat*.c -c
ar -rc libfat_io_m64e.a *.o
rm -f *.o

cd -


mkdir lib
mkdir include

mv release/*.a lib/
cp -a release/*.h include/

