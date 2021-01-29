#!/bin/sh

if [ -z "$1" ]; then
    EDKARCH=X64
    postfix=x64
elif [ "$1" = "ia32" ]; then
    EDKARCH=IA32
    postfix=ia32
    shift
elif [ "$1" = "aa64" ]; then
    EDKARCH=AARCH64
    postfix=aa64
    shift
fi

cd edk2-edk2-stable201911

rm -rf ./Conf/.cache
rm -f ./Conf/.AutoGenIdFile.txt

VTEFI_PATH=Build/MdeModule/RELEASE_GCC48/$EDKARCH/MdeModulePkg/Application/Ventoy/Ventoy/OUTPUT/Ventoy.efi
DST_PATH=../../INSTALL/ventoy/ventoy_${postfix}.efi

VTEFI_PATH2=Build/MdeModule/RELEASE_GCC48/$EDKARCH/MdeModulePkg/Application/VtoyUtil/VtoyUtil/OUTPUT/VtoyUtil.efi
DST_PATH2=../../INSTALL/ventoy/vtoyutil_${postfix}.efi

VTEFI_PATH3=Build/MdeModule/RELEASE_GCC48/$EDKARCH/MdeModulePkg/Application/VDiskChain/VDiskChain/OUTPUT/VDiskChain.efi
DST_PATH3=../../VDiskChain/Tool/vdiskchain_${postfix}.efi


rm -f $VTEFI_PATH
rm -f $DST_PATH
rm -f $VTEFI_PATH2
rm -f $DST_PATH2
rm -f $VTEFI_PATH3
[ -d ../../VDiskChain ] && rm -f $DST_PATH3

source ./edksetup.sh

if [ "$EDKARCH" = "AARCH64" ]; then    
    GCC48_AARCH64_PREFIX=aarch64-linux-gnu- \
    build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC48
else
    build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC48
fi

if [ -e $VTEFI_PATH ] && [ -e $VTEFI_PATH2 ] && [ -e $VTEFI_PATH3 ]; then
    echo -e '\n\n====================== SUCCESS ========================\n\n'    
    cp -a $VTEFI_PATH $DST_PATH
    cp -a $VTEFI_PATH2 $DST_PATH2
    [ -d ../../VDiskChain ] && cp -a $VTEFI_PATH3 $DST_PATH3
    cd ..
else
    echo -e '\n\n====================== FAILED ========================\n\n'
    cd ..
    exit 1
fi

