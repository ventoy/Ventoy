#!/bin/bash

VT_GRUB_DIR=$PWD

mkdir -p "$VT_GRUB_DIR/INSTALL"
rm -rf INSTALL
mkdir -p "$VT_GRUB_DIR/INSTALL"
rm -rf SRC
rm -rf NBP
rm -rf PXE

mkdir SRC
mkdir NBP
mkdir PXE

tar -xf grub-2.04.tar.xz -C ./SRC/

/bin/cp -a ./MOD_SRC/grub-2.04  ./SRC/
[ -f ./MOD_SRC/grub-2.04/install.sh ] && /bin/cp -f ./MOD_SRC/grub-2.04/install.sh ./SRC/grub-2.04/install.sh

cd ./SRC/grub-2.04


# build for x86_64-efi
echo '======== build grub2 for x86_64-efi ==============='
make distclean || true
PYTHON=python3 ./autogen.sh
./configure --disable-werror --with-platform=efi --prefix=$VT_GRUB_DIR/INSTALL/
make config-util.h || true
make -j$(nproc) || make
bash install.sh uefi


#build for i386-efi
echo '======== build grub2 for i386-efi ==============='
make distclean || true
PYTHON=python3 ./autogen.sh
./configure --disable-werror --target=i386 --with-platform=efi --prefix=$VT_GRUB_DIR/INSTALL/
make config-util.h || true
make -j$(nproc) || make
bash install.sh i386efi



#build for arm64 EFI
if which aarch64-linux-gnu-gcc >/dev/null 2>&1; then
echo '======== build grub2 for arm64-efi ==============='
PATH=$PATH:/opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin
make distclean || true
PYTHON=python3 ./autogen.sh
./configure --disable-werror --prefix=$VT_GRUB_DIR/INSTALL/ \
--target=aarch64 --with-platform=efi \
--host=x86_64-linux-gnu \
HOST_CC=x86_64-linux-gnu-gcc \
BUILD_CC=gcc \
TARGET_CC=aarch64-linux-gnu-gcc \
TARGET_OBJCOPY=aarch64-linux-gnu-objcopy \
TARGET_STRIP=aarch64-linux-gnu-strip TARGET_NM=aarch64-linux-gnu-nm \
TARGET_RANLIB=aarch64-linux-gnu-ranlib
make config-util.h || true
make -j$(nproc) || make
bash install.sh arm64
fi


#build for mips64el EFI
if which mips-linux-gnu-gcc >/dev/null 2>&1; then
echo '======== build grub2 for mips64el-efi ==============='
make distclean || true
PYTHON=python3 ./autogen.sh
./configure --disable-werror --prefix=$VT_GRUB_DIR/INSTALL/ \
--target=mips64el --with-platform=efi \
--host=x86_64-linux-gnu \
HOST_CC=x86_64-linux-gnu-gcc \
BUILD_CC=gcc \
TARGET_CC="mips-linux-gnu-gcc -mabi=64 -Wno-error=cast-align -Wno-error=misleading-indentation" \
TARGET_OBJCOPY=mips-linux-gnu-objcopy \
TARGET_STRIP=mips-linux-gnu-strip TARGET_NM=mips-linux-gnu-nm \
TARGET_RANLIB=mips-linux-gnu-ranlib
make config-util.h || true
make -j$(nproc) || make
bash install.sh mips64el
fi



# build for i386-pc
echo '======== build grub2 for i386-pc ==============='
make distclean || true
PYTHON=python3 ./autogen.sh
./configure --disable-werror --target=i386 --with-platform=pc --prefix=$VT_GRUB_DIR/INSTALL/
make config-util.h || true
make -j$(nproc) || make
bash install.sh

cd ../../

