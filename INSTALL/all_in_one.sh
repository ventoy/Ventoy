#!/bin/sh

VTOY_PATH=$PWD/..

cd $VTOY_PATH/DOC
sh installdietlibc.sh

cd $VTOY_PATH/GRUB2
sh buildgrub.sh || exit 1

cd $VTOY_PATH/IPXE
sh buildipxe.sh || exit 1

cd $VTOY_PATH/EDK2
sh buildedk.sh || exit 1

cd $VTOY_PATH/VtoyTool
sh build.sh || exit 1

cd $VTOY_PATH/vtoyfat/fat_io_lib
sh buildlib.sh

cd $VTOY_PATH/vtoyfat
sh build.sh || exit 1

cd $VTOY_PATH/vtoygpt
sh build.sh || exit 1

cd $VTOY_PATH/ExFAT
sh buidlibfuse.sh || exit 1
sh buidexfat.sh || exit 1
/bin/cp -a EXFAT/shared/mkexfatfs   $VTOY_PATH/INSTALL/tool/mkexfatfs_64
/bin/cp -a EXFAT/shared/mount.exfat-fuse   $VTOY_PATH/INSTALL/tool/mount.exfat-fuse_64


cd $VTOY_PATH/FUSEISO
sh build_libfuse.sh
sh build.sh

cd $VTOY_PATH/SQUASHFS/SRC
sh build_lz4.sh
sh build_lzma.sh
sh build_lzo.sh
sh build_zstd.sh

cd $VTOY_PATH/SQUASHFS/squashfs-tools-4.4/squashfs-tools
sh build.sh

cd $VTOY_PATH/VBLADE/vblade-master
sh build.sh

cd $VTOY_PATH/Ventoy2Disk/Ventoy2Disk/xz-embedded-20130513/userspace
make -f ventoy_makefile
strip --strip-all xzminidec
rm -f $VTOY_PATH/IMG/cpio/ventoy/tool/xzminidec
cp -a xzminidec $VTOY_PATH/IMG/cpio/ventoy/tool/xzminidec
make clean; rm -f *.o



cd $VTOY_PATH/INSTALL
sh ventoy_pack.sh || exit 1

echo -e '\n============== SUCCESS ==================\n'
