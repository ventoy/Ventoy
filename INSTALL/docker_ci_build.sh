#!/bin/sh

VTOY_PATH=$PWD/..

wget -q -P $VTOY_PATH/DOC/ https://www.fefe.de/dietlibc/dietlibc-0.34.tar.xz
wget -q -P $VTOY_PATH/DOC/ https://musl.libc.org/releases/musl-1.2.1.tar.gz
wget -q -P $VTOY_PATH/DOC/ https://musl.libc.org/releases/musl-1.2.1.tar.gz
wget -q -P $VTOY_PATH/GRUB2/ https://ftp.gnu.org/gnu/grub/grub-2.04.tar.xz
wget -q -O $VTOY_PATH/EDK2/edk2-edk2-stable201911.zip https://codeload.github.com/tianocore/edk2/zip/edk2-stable201911
wget -q -P /opt/ https://releases.linaro.org/components/toolchain/binaries/7.4-2019.02/aarch64-linux-gnu/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz
wget -q -P /opt/ https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--uclibc--stable-2020.08-1.tar.bz2

sh all_in_one.sh CI
