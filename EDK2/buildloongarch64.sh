#!/bin/sh

EDKARCH=LOONGARCH64
postfix=la64


cd edk2-edk2-stable202408

rm -rf ./Conf/.cache
rm -f ./Conf/.AutoGenIdFile.txt

VTEFI_PATH=Build/MdeModule/RELEASE_GCC/$EDKARCH/MdeModulePkg/Application/Ventoy/Ventoy/OUTPUT/Ventoy.efi
DST_PATH=../../INSTALL/ventoy/ventoy_${postfix}.efi

VTEFI_PATH2=Build/MdeModule/RELEASE_GCC/$EDKARCH/MdeModulePkg/Application/VtoyUtil/VtoyUtil/OUTPUT/VtoyUtil.efi
DST_PATH2=../../INSTALL/ventoy/vtoyutil_${postfix}.efi

VTEFI_PATH3=Build/MdeModule/RELEASE_GCC/$EDKARCH/MdeModulePkg/Application/VDiskChain/VDiskChain/OUTPUT/VDiskChain.efi
DST_PATH3=../../VDiskChain/Tool/vdiskchain_${postfix}.efi


rm -f $VTEFI_PATH
rm -f $DST_PATH
rm -f $VTEFI_PATH2
rm -f $DST_PATH2
rm -f $VTEFI_PATH3
[ -d ../../VDiskChain ] && rm -f $DST_PATH3

unset WORKSPACE
source ./edksetup.sh

build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC

if [ -e $VTEFI_PATH ] && [ -e $VTEFI_PATH2 ] && [ -e $VTEFI_PATH3 ]; then
    echo -e '\n\n====================== SUCCESS ========================\n\n'    
    cp -a $VTEFI_PATH $DST_PATH
    cp -a $VTEFI_PATH2 $DST_PATH2
    [ -d ../../VDiskChain ] && cp -a $VTEFI_PATH3 $DST_PATH3
    cd ..
else
    echo -e '\n\n====================== FAILED ========================\n\n'
    cd ..
fi

