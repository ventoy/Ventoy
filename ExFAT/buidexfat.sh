#!/bin/bash
# 
# For 32bit, for example CentOS 6.10 i386 
# automake 1.11.1 must update to automake 1.11.2
# pkg-config must be installed
#
#

if uname -a | egrep -q 'x86_64|amd64'; then
    opt=
else
    opt=-lrt
fi

CUR="$PWD"

if ! [ -e LIBFUSE ]; then
	./buidlibfuse.sh
fi

rm -rf EXFAT
mkdir -p EXFAT/shared
mkdir -p EXFAT/static


rm -rf exfat-1.3.0
unzip exfat-1.3.0.zip
sed "/printf.*VERSION/a\    if (access(\"/etc/initrd-release\", F_OK) >= 0) argv[0][0] = '@';"  -i exfat-1.3.0/fuse/main.c

cd exfat-1.3.0
autoreconf --install
./configure --prefix="$CUR" CFLAGS='-static -O2 -D_FILE_OFFSET_BITS=64' FUSE_CFLAGS="-I$CUR/LIBFUSE/include/" FUSE_LIBS="$CUR/LIBFUSE/lib/libfuse.a -pthread $opt -ldl"
make

strip --strip-all fuse/mount.exfat-fuse
strip --strip-all mkfs/mkexfatfs

cp fuse/mount.exfat-fuse ../EXFAT/static/mount.exfat-fuse
cp mkfs/mkexfatfs ../EXFAT/static/mkexfatfs

cd ..
rm -rf exfat-1.3.0

unzip exfat-1.3.0.zip
sed "/printf.*VERSION/a\    if (access(\"/etc/initrd-release\", F_OK) >= 0) argv[0][0] = '@';"  -i exfat-1.3.0/fuse/main.c


cd exfat-1.3.0
autoreconf --install
./configure --prefix="$CUR" CFLAGS='-O2 -D_FILE_OFFSET_BITS=64' FUSE_CFLAGS="-I$CUR/LIBFUSE/include/" FUSE_LIBS="$CUR/LIBFUSE/lib/libfuse.a -lpthread -ldl $opt"
make

strip --strip-all fuse/mount.exfat-fuse
strip --strip-all mkfs/mkexfatfs

cp fuse/mount.exfat-fuse ../EXFAT/shared/mount.exfat-fuse
cp mkfs/mkexfatfs ../EXFAT/shared/mkexfatfs

cd ..
rm -rf exfat-1.3.0




