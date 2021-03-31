#!/bin/bash

CUR="$PWD"

if ! [ -e LIBFUSE ]; then
	./buidlibfuse.sh
fi

rm -f EXFAT/shared/*


rm -rf exfat-1.3.0
unzip exfat-1.3.0.zip
sed "/printf.*VERSION/a\    if (access(\"/etc/initrd-release\", F_OK) >= 0) argv[0][0] = '@';"  -i exfat-1.3.0/fuse/main.c

cd exfat-1.3.0
autoreconf --install
./configure --prefix="$CUR" CFLAGS='-O2 -D_FILE_OFFSET_BITS=64' FUSE_CFLAGS="-I$CUR/LIBFUSE/include/" FUSE_LIBS="$CUR/LIBFUSE/lib/libfuse.a -lpthread -ldl"
make

strip --strip-all fuse/mount.exfat-fuse
strip --strip-all mkfs/mkexfatfs

cp fuse/mount.exfat-fuse ../EXFAT/shared/mount.exfat-fuse
cp mkfs/mkexfatfs ../EXFAT/shared/mkexfatfs

cd ..
rm -rf exfat-1.3.0




