FROM centos:7

RUN yum -y install \
        libXpm net-tools bzip2 wget vim gcc gcc-c++ samba dos2unix glibc-devel glibc.i686 glibc-devel.i686 \
        mpfr.i686 mpfr-devel.i686 zlib.i686 rsync autogen autoconf automake libtool gettext* bison binutils \
        flex device-mapper-devel SDL libpciaccess libusb freetype freetype-devel gnu-free-* qemu-* virt-* \
        libvirt* vte* NetworkManager-bluetooth brlapi fuse-devel dejavu* gnu-efi* pesign shim \
        iscsi-initiator-utils grub2-tools zip nasm acpica-tools glibc-static zlib-static xorriso

CMD cd /ventoy \
    && wget -P DOC/ https://www.fefe.de/dietlibc/dietlibc-0.34.tar.xz \
    && wget -P DOC/ https://musl.libc.org/releases/musl-1.2.1.tar.gz \
    && wget -P GRUB2/ https://ftp.gnu.org/gnu/grub/grub-2.04.tar.xz \
    && wget -O EDK2/edk2-edk2-stable201911.zip https://codeload.github.com/tianocore/edk2/zip/edk2-stable201911 \
    && wget -P /opt/ https://releases.linaro.org/components/toolchain/binaries/7.4-2019.02/aarch64-linux-gnu/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz  \
    && wget -P /opt/ https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/aarch64--uclibc--stable-2020.08-1.tar.bz2  \
    && cd INSTALL && ls -la && sh all_in_one.sh CI
