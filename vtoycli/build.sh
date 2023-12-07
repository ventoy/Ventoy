#!/bin/sh

# Define source files
SRCS="vtoycli.c vtoyfat.c vtoygpt.c crc32.c partresize.c"

# Function to compile for different architectures
compile_vtoycli() {
    local compiler=$1
    local lib=$2
    local output=$3
    local extra_flags=$4

    $compiler $extra_flags -D_FILE_OFFSET_BITS=64 $SRCS -Ifat_io_lib/include $lib -o $output
}

# Remove old binaries
rm -f vtoycli_64 vtoycli_32 vtoycli_aa64 vtoycli_m64e

# Compile for different architectures
compile_vtoycli "gcc -specs /usr/local/musl/lib/musl-gcc.specs -Os -static" "fat_io_lib/lib/libfat_io_64.a" "vtoycli_64"
compile_vtoycli "/opt/diet32/bin/diet -Os gcc -m32" "fat_io_lib/lib/libfat_io_32.a" "vtoycli_32"
compile_vtoycli "aarch64-buildroot-linux-uclibc-gcc -static -O2" "fat_io_lib/lib/libfat_io_aa64.a" "vtoycli_aa64"
compile_vtoycli "mips64el-linux-musl-gcc -mips64r2 -mabi=64 -static -O2" "fat_io_lib/lib/libfat_io_m64e.a" "vtoycli_m64e"

# Check if all binaries are created and move them
if [ -e vtoycli_64 ] && [ -e vtoycli_32 ] && [ -e vtoycli_aa64 ] && [ -e vtoycli_m64e ]; then
    echo -e "\n===== success =======\n"
    
    # Strip all binaries
    strip --strip-all vtoycli_32 vtoycli_64
    aarch64-buildroot-linux-uclibc-strip --strip-all vtoycli_aa64
    mips64el-linux-musl-strip --strip-all vtoycli_m64e

    # Move binaries to respective directories
    [ -d ../INSTALL/tool/i386/ ] && mv vtoycli_32 ../INSTALL/tool/i386/vtoycli
    [ -d ../INSTALL/tool/x86_64/ ] && mv vtoycli_64 ../INSTALL/tool/x86_64/vtoycli
    [ -d ../INSTALL/tool/aarch64/ ] && mv vtoycli_aa64 ../INSTALL/tool/aarch64/vtoycli
    [ -d ../INSTALL/tool/mips64el/ ] && mv vtoycli_m64e ../INSTALL/tool/mips64el/vtoycli
else
    echo -e "\n===== failed =======\n"
    exit 1
fi
