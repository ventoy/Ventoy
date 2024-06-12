#!/bin/sh

# Remove old EDK2 directory and unzip new one
rm -rf edk2-edk2-stable201911
unzip edk2-edk2-stable201911.zip > /dev/null

# Copy modified EDK2 files
/bin/cp -a ./edk2_mod/edk2-edk2-stable201911 ./

# Build BaseTools
cd edk2-edk2-stable201911 || exit 1
make -j 4 -C BaseTools/
cd ..

# Function to build EDK2 for different architectures
build_edk2() {
    local arch=$1
    echo "======== build EDK2 for $arch-efi ==============="
    sh ./build.sh "$arch" || exit 1
}

# Build for different architectures
build_edk2 "ia32"
build_edk2 "aa64"
build_edk2 "" # default to x86_64
