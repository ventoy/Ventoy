#!/bin/bash

# Remove existing files
rm -f vtoygpt_64
rm -f vtoygpt_32
rm -f vtoygpt_aa64

# Compile for x86_64
/opt/diet64/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64 vtoygpt.c crc32.c -o vtoygpt_64

# Compile for i386
/opt/diet32/bin/diet -Os gcc -D_FILE_OFFSET_BITS=64 -m32 vtoygpt.c crc32.c -o vtoygpt_32

# Compile for aarch64
aarch64-buildroot-linux-uclibc-gcc -Os -static -D_FILE_OFFSET_BITS=64 vtoygpt.c crc32.c -o vtoygpt_aa64

# Compile for mips64el
mips64el-linux-musl-gcc -mips64r2 -mabi=64 -static -Os -D_FILE_OFFSET_BITS=64 vtoygpt.c crc32.c -o vtoygpt_m64e

# Check compilation success
if [ -e vtoygpt_64 ] && [ -e vtoygpt_32 ] && [ -e vtoygpt_aa64 ] && [ -e vtoygpt_m64e ]; then
    echo -e '\n############### SUCCESS ###############\n'
    
    # Move files to installation directories
    mv vtoygpt_64 ../INSTALL/tool/x86_64/vtoygpt
    mv vtoygpt_32 ../INSTALL/tool/i386/vtoygpt
    
    # Strip symbols and move aarch64 executable
    aarch64-buildroot-linux-uclibc-strip --strip-all vtoygpt_aa64
    mv vtoygpt_aa64 ../INSTALL/tool/aarch64/vtoygpt
    
    # Strip symbols and move mips64el executable
    mips64el-linux-musl-strip --strip-all vtoygpt_m64e
    mv vtoygpt_m64e ../INSTALL/tool/mips64el/vtoygpt
    
    echo -e 'Compilation successful. Executables moved to the installation directory.'
else
    echo -e '\n############### FAILED ################\n'
    exit 1
fi
