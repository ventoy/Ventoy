#!/bin/sh

VTOY_PATH=$PWD/..

cilog() {
    datestr=$(date +"%Y/%m/%d %H:%M:%S")
    echo "$datestr $*"
}

LOG=$VTOY_PATH/DOC/build.log
[ -f $LOG ] && rm -f $LOG

cd $VTOY_PATH/DOC
cilog "prepare_env ..."
sh prepare_env.sh

export PATH=$PATH:/opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin:/opt/aarch64--uclibc--stable-2020.08-1/bin:/opt/mips-loongson-gcc7.3-linux-gnu/2019.06-29/bin/:/opt/mips64el-linux-musl-gcc730/bin/

cilog "build grub2 ..."
cd $VTOY_PATH/GRUB2
sh buildgrub.sh >> $LOG 2>&1 || exit 1

cilog "build ipxe ..."
cd $VTOY_PATH/IPXE
sh buildipxe.sh >> $LOG 2>&1 || exit 1

cilog "build edk2 ..."
cd $VTOY_PATH/EDK2
sh buildedk.sh >> $LOG 2>&1 || exit 1



#
# We almost rarely modifiy these code, so no need to build them everytime
# If you want to rebuild them, just uncomment them.
#

#cd $VTOY_PATH/VtoyTool
#sh build.sh || exit 1

#cd $VTOY_PATH/vtoycli/fat_io_lib
#sh buildlib.sh

#cd $VTOY_PATH/vtoycli
#sh build.sh || exit 1

#cd $VTOY_PATH/FUSEISO
#sh build_libfuse.sh
#sh build.sh


# cd $VTOY_PATH/ExFAT
# sh buidlibfuse.sh || exit 1
# sh buidexfat.sh || exit 1
# /bin/cp -a EXFAT/shared/mkexfatfs   $VTOY_PATH/INSTALL/tool/mkexfatfs_64
# /bin/cp -a EXFAT/shared/mount.exfat-fuse   $VTOY_PATH/INSTALL/tool/mount.exfat-fuse_64


# cd $VTOY_PATH/SQUASHFS/SRC
# sh build_lz4.sh
# sh build_lzma.sh
# sh build_lzo.sh
# sh build_zstd.sh

# cd $VTOY_PATH/SQUASHFS/squashfs-tools-4.4/squashfs-tools
# sh build.sh

# cd $VTOY_PATH/VBLADE/vblade-master
# sh build.sh

cd $VTOY_PATH/INSTALL

if [ "$1" = "CI" ]; then
    Ver=$(date +%m%d%H%M)
    sed "s/VENTOY_VERSION=.*/VENTOY_VERSION=\"$Ver\"/"  -i ./grub/grub.cfg
fi

cilog "packing ventoy-$Ver ..."
sh ventoy_pack.sh $1 >> $LOG 2>&1 || exit 1

echo -e '\n============== SUCCESS ==================\n'
