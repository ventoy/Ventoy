#!/bin/sh

VTOY_PATH=$PWD/..

date +"%Y/%m/%d %H:%M:%S"
echo downloading environment ...

wget -q -P $VTOY_PATH/DOC/ https://github.com/ventoy/vtoytoolchain/releases/download/1.0/dietlibc-0.34.tar.xz
wget -q -P $VTOY_PATH/DOC/ https://github.com/ventoy/vtoytoolchain/releases/download/1.0/musl-1.2.1.tar.gz
wget -q -P $VTOY_PATH/GRUB2/ https://github.com/ventoy/vtoytoolchain/releases/download/1.0/grub-2.04.tar.xz
wget -q -O $VTOY_PATH/EDK2/edk2-edk2-stable201911.zip https://codeload.github.com/tianocore/edk2/zip/edk2-stable201911
wget -q -P /opt/ https://github.com/ventoy/vtoytoolchain/releases/download/1.0/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz
wget -q -P /opt/ https://github.com/ventoy/vtoytoolchain/releases/download/1.0/aarch64--uclibc--stable-2020.08-1.tar.bz2
wget -q -P /opt/ https://github.com/ventoy/vtoytoolchain/releases/download/1.0/mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz

date +"%Y/%m/%d %H:%M:%S"
echo downloading environment finish...

sh all_in_one.sh CI
