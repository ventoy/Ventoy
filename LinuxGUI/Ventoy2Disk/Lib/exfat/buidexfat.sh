#!/bin/sh

CUR="$PWD"

rm -rf src
mkdir -p src/libexfat
mkdir -p src/mkfs

rm -rf exfat-1.3.0
unzip exfat-1.3.0.zip

cd exfat-1.3.0
autoreconf --install
./configure --prefix="$CUR" CFLAGS='-O2 -D_FILE_OFFSET_BITS=64'
make

cp -a libexfat/*.c ../src/libexfat/
cp -a libexfat/*.h ../src/libexfat/
cp -a mkfs/*.c ../src/mkfs/
cp -a mkfs/*.h ../src/mkfs/
rm -f ../src/libexfat/log.c

cd ..
rm -rf exfat-1.3.0

mv src/mkfs/main.c src/mkfs/mkexfat_main.c
sed 's/<exfat.h>/"exfat.h"/g' -i src/mkfs/mkexfat_main.c
sed 's/<exfat.h>/"exfat.h"/g' -i src/mkfs/mkexfat.h
sed 's/int main/int mkexfat_main/g' -i src/mkfs/mkexfat_main.c

