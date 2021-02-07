#!/bin/sh

rm -rf edk2-edk2-stable201911

unzip edk2-edk2-stable201911.zip > /dev/null

/bin/cp -a ./edk2_mod/edk2-edk2-stable201911  ./

cd edk2-edk2-stable201911
make -j 4 -C BaseTools/
cd ..

echo '======== build EDK2 for i386-efi ==============='
sh ./build.sh ia32 || exit 1

echo '======== build EDK2 for arm64-efi ==============='
sh ./build.sh aa64 || exit 1

echo '======== build EDK2 for x86_64-efi ==============='
sh ./build.sh      || exit 1

