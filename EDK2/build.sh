#!/bin/sh

# Set architecture based on input argument
case "$1" in
    "ia32")
        EDKARCH=IA32
        postfix=ia32
        ;;
    "aa64")
        EDKARCH=AARCH64
        postfix=aa64
        ;;
    *)
        EDKARCH=X64
        postfix=x64
        ;;
esac

cd edk2-edk2-stable201911 || exit 1

# Clean up configuration and cache files
rm -rf ./Conf/.cache
rm -f ./Conf/.AutoGenIdFile.txt

# Define paths for EFI files
VTEFI_PATH=Build/MdeModule/RELEASE_GCC48/$EDKARCH/MdeModulePkg/Application/Ventoy/Ventoy/OUTPUT/Ventoy.efi
DST_PATH=../../INSTALL/ventoy/ventoy_${postfix}.efi

VTEFI_PATH2=Build/MdeModule/RELEASE_GCC48/$EDKARCH/MdeModulePkg/Application/VtoyUtil/VtoyUtil/OUTPUT/VtoyUtil.efi
DST_PATH2=../../INSTALL/ventoy/vtoyutil_${postfix}.efi

VTEFI_PATH3=Build/MdeModule/RELEASE_GCC48/$EDKARCH/MdeModulePkg/Application/VDiskChain/VDiskChain/OUTPUT/VDiskChain.efi
DST_PATH3=../../VDiskChain/Tool/vdiskchain_${postfix}.efi

# Remove old EFI files
rm -f $VTEFI_PATH $DST_PATH $VTEFI_PATH2 $DST_PATH2 $VTEFI_PATH3
[ -d ../../VDiskChain ] && rm -f $DST_PATH3

# Setup build environment
unset WORKSPACE
source ./edksetup.sh

# Build based on architecture
if [ "$EDKARCH" = "AARCH64" ]; then    
    GCC48_AARCH64_PREFIX=aarch64-linux-gnu- \
    build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC48
else
    build -p MdeModulePkg/MdeModulePkg.dsc -a $EDKARCH -b RELEASE -t GCC48
fi

# Check if build was successful and copy files
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
