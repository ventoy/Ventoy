FROM rockylinux/rockylinux:9.7

RUN dnf -y install dnf-plugins-core && \
    dnf config-manager --set-enabled crb && \
    dnf -y --setopt=install_weak_deps=False install \
        libXpm net-tools bzip2 wget vim-enhanced gcc gcc-c++ samba dos2unix \
        glibc-devel glibc.i686 glibc-devel.i686 \
        mpfr.i686 mpfr-devel.i686 zlib.i686 \
        rsync python-unversioned-command autoconf automake libtool gettext gettext-devel bison binutils \
        flex device-mapper-multipath-devel SDL libpciaccess libusb \
        freetype freetype-devel qemu-* virt-* libvirt* vte* \
        NetworkManager-bluetooth brlapi dejavu* gnu-efi* \
        fuse3-devel libuuid-devel pesign shim iscsi-initiator-utils grub2-tools \
        zip nasm acpica-tools glibc-static xorriso lz4 squashfs-tools && \
    dnf clean all && \
    rm -rf /var/cache/dnf

CMD ["bash", "-lc", "set -euo pipefail; src=/ventoy; work=/tmp/ventoy; rm -rf \"$work\"; mkdir -p \"$work\"; rsync -a --delete --exclude '.git/' --exclude '.tmpcmp/' --exclude '.tmp_cmp/' --exclude 'GRUB2/INSTALL/' --exclude 'GRUB2/INSTALL_all/' --exclude 'GRUB2/MIPSDBG/' --exclude 'GRUB2/MIPSDBG2/' --exclude 'GRUB2/grub206*.log' --exclude 'GRUB2/grub-2.06.tar.xz' --exclude 'EDK2/edk2-stable201911/' --exclude 'EDK2/buildedk_armgnu.log' --exclude 'EDK2/buildedk_armgnu_rerun.log' --exclude 'DOC/build.log' --exclude 'DOC/docker_compose_full.log' --exclude 'INSTALL/sha256.txt' --exclude 'INSTALL/ventoy-*' \"$src\"/ \"$work\"/; find \"$work\" -type f -name '*.sh' -print0 | xargs -0 dos2unix -q; cd \"$work/INSTALL\"; ls -la; status=0; sh docker_ci_build.sh || status=$?; cp -a \"$work/INSTALL\"/ventoy-* \"$src/INSTALL\"/ 2>/dev/null || true; cp -a \"$work/INSTALL\"/sha256.txt \"$src/INSTALL\"/ 2>/dev/null || true; cp -a \"$work/DOC\"/build.log \"$src/DOC\"/ 2>/dev/null || true; exit $status"]
