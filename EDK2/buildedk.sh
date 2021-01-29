#!/bin/sh

rm -rf edk2-edk2-stable201911

unzip edk2-edk2-stable201911.zip

/bin/cp -a ./edk2_mod/edk2-edk2-stable201911  ./

cd edk2-edk2-stable201911
make -j 4 -C BaseTools/
cd ..

sh ./build.sh ia32 || exit 1
sh ./build.sh aa64 || exit 1
sh ./build.sh      || exit 1

