FROM centos:7

RUN yum -y install \
        libXpm net-tools bzip2 wget vim gcc gcc-c++ samba dos2unix glibc-devel glibc.i686 glibc-devel.i686 \
        mpfr.i686 mpfr-devel.i686 zlib.i686 rsync autogen autoconf automake libtool gettext* bison binutils \
        flex device-mapper-devel SDL libpciaccess libusb freetype freetype-devel gnu-free-* qemu-* virt-* \
        libvirt* vte* NetworkManager-bluetooth brlapi fuse-devel dejavu* gnu-efi* pesign shim \
        iscsi-initiator-utils grub2-tools zip nasm acpica-tools glibc-static zlib-static

CMD cd /ventoy \
    && curl -L https://www.fefe.de/dietlibc/dietlibc-0.34.tar.xz > DOC/dietlibc-0.34.tar.xz \
    && curl -L https://ftp.gnu.org/gnu/grub/grub-2.04.tar.xz > GRUB2/grub-2.04.tar.xz \
    && curl -L https://codeload.github.com/tianocore/edk2/zip/edk2-stable201911 > EDK2/edk2-edk2-stable201911.zip \
    && curl -L https://codeload.github.com/relan/exfat/zip/v1.3.0 > ExFAT/exfat-1.3.0.zip \
    && curl -L https://codeload.github.com/libfuse/libfuse/zip/fuse-2.9.9 > ExFAT/libfuse-fuse-2.9.9.zip \
    && cd INSTALL && ls -la && sh all_in_one.sh
