#!/bin/sh

VTOY_PATH=$PWD/../

cd wimboot-2.9.0/src

make clean
make -j 16 wimboot.x86_64 wimboot.i386.efi

rm -f *.xz
xz wimboot.x86_64
xz wimboot.i386.efi

rm -f $VTOY_PATH/INSTALL/ventoy/wimboot.x86_64.xz
rm -f $VTOY_PATH/INSTALL/ventoy/wimboot.i386.efi.xz
cp -a wimboot.x86_64.xz $VTOY_PATH/INSTALL/ventoy/
cp -a wimboot.i386.efi.xz $VTOY_PATH/INSTALL/ventoy/

make clean
cd ../../
