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

VTEFI_PATH=Build/MdeModule/RELEASE_GCC48/$EDKARCH/MdeModulePkg/Application/VtoyShim/VtoyShim/OUTPUT/VtoyShim.efi
DST_PATH=../../INSTALL/EFI/BOOT/fb${postfix}.efi


rm -f $VTEFI_PATH
rm -f $DST_PATH

unset WORKSPACE
source ./edksetup.sh

if [ "$EDKARCH" = "AARCH64" ]; then    
    PATH=$PATH:/opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin \
    GCC48_AARCH64_PREFIX=aarch64-linux-gnu- \
    build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC48  -m MdeModulePkg/Application/VtoyShim/VtoyShim.inf
else
    build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC48  -m MdeModulePkg/Application/VtoyShim/VtoyShim.inf
fi

if [ -e $VTEFI_PATH ]; then
    objcopy \
        --add-section .sbat="MdeModulePkg/Application/VtoyShim/sbat.csv" \
        --set-section-flags .sbat=alloc,load,readonly,data \
        "$VTEFI_PATH" "$DST_PATH"
        
    objcopy --adjust-section-vma .sbat=0x1000 "$DST_PATH"

    echo -e '\n\n====================== SUCCESS ========================\n\n'    

    cd ..
else
    echo -e '\n\n====================== FAILED ========================\n\n'
    cd ..
    exit 1
fi

