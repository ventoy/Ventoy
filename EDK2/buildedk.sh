#!/bin/sh

rm -rf edk2-edk2-stable201911

unzip edk2-edk2-stable201911.zip

/bin/cp -a ./edk2_mod/edk2-edk2-stable201911  ./

cd edk2-edk2-stable201911

VTEFI_PATH=Build/MdeModule/RELEASE_GCC48/X64/MdeModulePkg/Application/Ventoy/Ventoy/OUTPUT/Ventoy.efi
DST_PATH=../../INSTALL/ventoy/ventoy_x64.efi

VTEFI_PATH2=Build/MdeModule/RELEASE_GCC48/IA32/MdeModulePkg/Application/Ventoy/Ventoy/OUTPUT/Ventoy.efi
DST_PATH2=../../INSTALL/ventoy/ventoy_ia32.efi

rm -f $VTEFI_PATH
rm -f $DST_PATH
rm -f $VTEFI_PATH2
rm -f $DST_PATH2

make -j 4 -C BaseTools/

source ./edksetup.sh
build -p MdeModulePkg/MdeModulePkg.dsc -a  X64 -b RELEASE -t GCC48
build -p MdeModulePkg/MdeModulePkg.dsc -a IA32 -b RELEASE -t GCC48

if [ -e $VTEFI_PATH ] && [ -e $VTEFI_PATH2 ]; then
    echo -e '\n\n====================== SUCCESS ========================\n\n'    
    cp -a $VTEFI_PATH  $DST_PATH
    cp -a $VTEFI_PATH2 $DST_PATH2
    cd ..
else
    echo -e '\n\n====================== FAILED ========================\n\n'
    cd ..
    exit 1
fi



