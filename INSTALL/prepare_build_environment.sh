#!/bin/sh
BASE_DIR=$(dirname "${0}")
BASE_DIR="${BASE_DIR}/.."

download() {
  if [ -f ${2} ]; then
    echo "===> ${2} already present"
  else
    echo "===> Downloading ${1} to ${2}"
    curl -L ${1} -o ${2}
  fi
}

echo "========================================="
echo "      Installing RPM dependencies        "
echo "========================================="
if [ "$(uname -p)" == "i686" ]; then
  IA32_DEPENDENCIES=" glibc.i686 glibc-devel.i686 mpfr.i686 mpfr-devel.i686 zlib.i686"
fi
yum -y install ${IA32_DEPENDENCIES} \
               libXpm net-tools bzip2 wget vim gcc gcc-c++ samba dos2unix glibc-devel \
               rsync autogen autoconf automake libtool gettext* bison binutils \
               flex device-mapper-devel SDL libpciaccess libusb freetype freetype-devel gnu-free-* qemu-* virt-* \
               libvirt* vte* NetworkManager-bluetooth brlapi fuse-devel dejavu* gnu-efi* pesign shim \
               iscsi-initiator-utils grub2-tools zip nasm acpica-tools glibc-static zlib-static xorriso

echo "========================================="
echo "    Downloading external dependencies    "
echo "========================================="
download "https://www.fefe.de/dietlibc/dietlibc-0.34.tar.xz" "${BASE_DIR}/DOC/dietlibc-0.34.tar.xz"
download "https://musl.libc.org/releases/musl-1.2.1.tar.gz" "${BASE_DIR}/DOC/musl-1.2.1.tar.gz"
download "https://ftp.gnu.org/gnu/grub/grub-2.04.tar.xz" "${BASE_DIR}/GRUB2/grub-2.04.tar.xz"
download "https://codeload.github.com/tianocore/edk2/zip/edk2-stable201911" "${BASE_DIR}/EDK2/edk2-edk2-stable201911.zip"
download "https://codeload.github.com/relan/exfat/zip/v1.3.0" "${BASE_DIR}/ExFAT/exfat-1.3.0.zip"
download "https://codeload.github.com/libfuse/libfuse/zip/fuse-2.9.9" "${BASE_DIR}/ExFAT/libfuse-fuse-2.9.9.zip"
download "https://releases.linaro.org/components/toolchain/binaries/7.4-2019.02/aarch64-linux-gnu/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz" "/opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz"
download "https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--uclibc--stable-2020.08-1.tar.bz2" "/opt/aarch64--uclibc--stable-2020.08-1.tar.bz2"
download "http://ftp.loongnix.org/toolchain/gcc/release/mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz" "/opt/mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz"
download "https://github.com/ventoy/musl-cross-make/releases/download/latest/output.tar.bz2" "/opt/output.tar.bz2"
download "http://www.tinycorelinux.net/11.x/x86_64/release/distribution_files/vmlinuz64" "${BASE_DIR}/LiveCD/ISO/EFI/boot/vmlinuz64"
download "http://www.tinycorelinux.net/11.x/x86_64/release/distribution_files/corepure64.gz" "${BASE_DIR}/LiveCD/ISO/EFI/boot/corepure64.gz"
download "http://www.tinycorelinux.net/11.x/x86_64/release/distribution_files/modules64.gz" "${BASE_DIR}/LiveCD/ISO/EFI/boot/modules64.gz"


cd ${BASE_DIR}/DOC/
tar xf musl-1.2.1.tar.gz
cd musl-1.2.1
./configure && make install
cd ${OLDPWD}

tar xf /opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz  -C /opt
tar xf /opt/aarch64--uclibc--stable-2020.08-1.tar.bz2  -C /opt
tar xf /opt/output.tar.bz2  -C /opt
mv /opt/output /opt/mips64el-linux-musl-gcc730
