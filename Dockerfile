FROM centos:7

RUN sed -i \
    -e 's/^mirrorlist/#mirrorlist/' \
    -e 's/^#baseurl/baseurl/' \
    -e 's/mirror\.centos\.org/vault.centos.org/' \
    /etc/yum.repos.d/*.repo && \
    yum -y -q install \
        libXpm net-tools bzip2 wget vim gcc gcc-c++ samba dos2unix glibc-devel glibc.i686 glibc-devel.i686 \
        mpfr.i686 mpfr-devel.i686 rsync autogen autoconf automake libtool gettext* bison binutils \
        flex device-mapper-devel SDL libpciaccess libusb freetype freetype-devel gnu-free-* qemu-* virt-* \
        libvirt* vte* NetworkManager-bluetooth brlapi fuse-devel dejavu* gnu-efi* pesign shim \
        iscsi-initiator-utils grub2-tools zip nasm acpica-tools glibc-static zlib-static xorriso lz4 squashfs-tools

CMD cd /ventoy/INSTALL && ls -la && sh docker_ci_build.sh    
