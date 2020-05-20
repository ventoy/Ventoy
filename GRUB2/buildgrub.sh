#!/bin/bash

VT_GRUB_DIR=$PWD

rm -rf INSTALL
rm -rf SRC
rm -rf NBP
rm -rf PXE

mkdir SRC
mkdir NBP
mkdir PXE

tar -xvf grub-2.04.tar.xz -C ./SRC/

/bin/cp -a ./MOD_SRC/grub-2.04  ./SRC/

cd ./SRC/grub-2.04

# build for Legacy BIOS 
./autogen.sh
./configure  --prefix=$VT_GRUB_DIR/INSTALL/
make -j 16
sh install.sh

# build for UEFI
make distclean
./autogen.sh
./configure  --with-platform=efi --prefix=$VT_GRUB_DIR/INSTALL/
make -j 16
sh install.sh  uefi


cd ../../

