#!/bin/bash

VENTOY_PATH=$PWD/../
TOOLS="$VENTOY_PATH/build/out/tools"
INITRAMFS="$VENTOY_PATH/build/out/initramfs"

# Prefer a binary built by build/*.sh, and fall back to the legacy blob.
take() {
    dest=$1
    shift
    for src in "$@"; do
        if [ -f "$src" ]; then
            cp -a "$src" "$dest"
            return 0
        fi
    done
    echo "mkcpio: missing $dest" >&2
    return 1
}

if [ -d cpio_tmp ]; then
    rm -rf cpio_tmp
fi


############### cpio ############
rm -f ventoy.cpio ventoy_x86.cpio ventoy_arm64.cpio ventoy_mips64.cpio ventoy_loongarch64.cpio 

cp -a cpio	cpio_tmp
chmod -R 777 cpio_tmp

cd cpio_tmp
rm -f init
ln -s sbin/init init
ln -s sbin/init linuxrc

cd ventoy

find ./loop | cpio  -o -H newc --owner=root:root >loop.cpio
xz loop.cpio
rm -rf loop

xz ventoy_chain.sh
xz ventoy_loop.sh

find ./hook | cpio  -o -H newc --owner=root:root >hook.cpio
xz hook.cpio
rm -rf hook
cd ..

find .| cpio  -o -H newc --owner=root:root >../ventoy.cpio

cd ..
rm -rf cpio_tmp




########## cpio_x86 ##############
cp -a cpio_x86	cpio_tmp
chmod -R 777 cpio_tmp

cd cpio_tmp/ventoy

take tool/dmsetup32 "$TOOLS/i386/dmsetup" "$VENTOY_PATH/DMSETUP/dmsetup32"
take tool/dmsetup64 "$TOOLS/x86_64/dmsetup" "$VENTOY_PATH/DMSETUP/dmsetup64"
take tool/unsquashfs_32 "$TOOLS/i386/unsquashfs" "$VENTOY_PATH/SQUASHFS/unsquashfs_32"
take tool/unsquashfs_64 "$TOOLS/x86_64/unsquashfs" "$VENTOY_PATH/SQUASHFS/unsquashfs_64"
take tool/vtoy_fuse_iso_32 "$TOOLS/i386/vtoy_fuse_iso" "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_32"
take tool/vtoy_fuse_iso_64 "$TOOLS/x86_64/vtoy_fuse_iso" "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_64"
cp -a $VENTOY_PATH/VtoyTool/vtoytool tool/
rm -f tool/vtoytool/00/vtoytool_aa64
rm -f tool/vtoytool/00/vtoytool_m64e
take tool/vtoytool/00/vtoytool_32 "$TOOLS/i386/vtoytool" "$VENTOY_PATH/VtoyTool/vtoytool/00/vtoytool_32"
take tool/vtoytool/00/vtoytool_64 "$TOOLS/x86_64/vtoytool" "$VENTOY_PATH/VtoyTool/vtoytool/00/vtoytool_64"
take tool/vblade_32 "$TOOLS/i386/vblade" "$VENTOY_PATH/VBLADE/vblade-master/vblade_32"
take tool/vblade_64 "$TOOLS/x86_64/vblade" "$VENTOY_PATH/VBLADE/vblade-master/vblade_64"

take tool/lunzip32 "$TOOLS/i386/lunzip" "$VENTOY_PATH/LZIP/lunzip32"
take tool/lunzip64 "$TOOLS/x86_64/lunzip" "$VENTOY_PATH/LZIP/lunzip64"
take tool/lz4cat "$TOOLS/i386/lz4cat" tool/lz4cat
take tool/lz4cat64 "$TOOLS/x86_64/lz4cat" tool/lz4cat64
take tool/zstdcat "$TOOLS/i386/zstdcat" tool/zstdcat
take tool/zstdcat64 "$TOOLS/x86_64/zstdcat" tool/zstdcat64

if [ -f $VENTOY_PATH/cryptsetup/veritysetup32 ]; then
    cp -a $VENTOY_PATH/cryptsetup/veritysetup32 tool/
fi
if [ -f $VENTOY_PATH/cryptsetup/veritysetup64 ]; then
    cp -a $VENTOY_PATH/cryptsetup/veritysetup64 tool/
fi

if [ -f "$INITRAMFS/x86_64/busybox.xz" ]; then
    cp -a "$INITRAMFS/x86_64/busybox.xz" busybox/busybox64.xz
fi
if [ -f "$INITRAMFS/i386/busybox.xz" ]; then
    cp -a "$INITRAMFS/i386/busybox.xz" busybox/busybox32.xz
fi
if [ -f "$INITRAMFS/x86_64/vtchmod" ]; then
    cp -a "$INITRAMFS/x86_64/vtchmod" busybox/vtchmod64
fi
if [ -f "$INITRAMFS/i386/vtchmod" ]; then
    cp -a "$INITRAMFS/i386/vtchmod" busybox/vtchmod32
fi

chmod -R 777 ./tool

find ./tool | cpio  -o -H newc --owner=root:root >tool.cpio
xz tool.cpio
rm -rf tool

cd ..
find .| cpio  -o -H newc --owner=root:root >../ventoy_x86.cpio

cd ..
rm -rf cpio_tmp


########## cpio_arm64 ##############
cp -a cpio_arm64	cpio_tmp
chmod -R 777 cpio_tmp
cp -a cpio_x86/ventoy/tool/*.sh cpio_tmp/ventoy/tool/

cd cpio_tmp/ventoy

take tool/dmsetupaa64 "$TOOLS/aarch64/dmsetup" "$VENTOY_PATH/DMSETUP/dmsetupaa64"
take tool/unsquashfs_aa64 "$TOOLS/aarch64/unsquashfs" "$VENTOY_PATH/SQUASHFS/unsquashfs_aa64"
take tool/vtoy_fuse_iso_aa64 "$TOOLS/aarch64/vtoy_fuse_iso" "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_aa64"
cp -a $VENTOY_PATH/VtoyTool/vtoytool tool/
rm -f tool/vtoytool/00/vtoytool_32
rm -f tool/vtoytool/00/vtoytool_64
rm -f tool/vtoytool/00/vtoytool_m64e
take tool/vtoytool/00/vtoytool_aa64 "$TOOLS/aarch64/vtoytool" "$VENTOY_PATH/VtoyTool/vtoytool/00/vtoytool_aa64"
take tool/vblade_aa64 "$TOOLS/aarch64/vblade" "$VENTOY_PATH/VBLADE/vblade-master/vblade_aa64"

take tool/lunzipaa64 "$TOOLS/aarch64/lunzip" "$VENTOY_PATH/LZIP/lunzipaa64"
take tool/lz4cataa64 "$TOOLS/aarch64/lz4cat" tool/lz4cataa64
take tool/zstdcataa64 "$TOOLS/aarch64/zstdcat" tool/zstdcataa64

if [ -f "$INITRAMFS/aarch64/busybox.xz" ]; then
    cp -a "$INITRAMFS/aarch64/busybox.xz" busybox/busyboxaa64.xz
fi
if [ -f "$INITRAMFS/aarch64/vtchmod" ]; then
    cp -a "$INITRAMFS/aarch64/vtchmod" busybox/vtchmodaa64
fi
if [ -f "$INITRAMFS/aarch64/xzminidec" ]; then
    cp -a "$INITRAMFS/aarch64/xzminidec" busybox/xzminidecaa64
fi

chmod -R 777 ./tool

find ./tool | cpio  -o -H newc --owner=root:root >tool.cpio
xz tool.cpio
rm -rf tool

cd ..
find .| cpio  -o -H newc --owner=root:root >../ventoy_arm64.cpio

cd ..
rm -rf cpio_tmp



########## cpio_mips64 ##############
cp -a cpio_mips64	cpio_tmp
chmod -R 777 cpio_tmp
cp -a cpio_x86/ventoy/tool/*.sh cpio_tmp/ventoy/tool/

cd cpio_tmp/ventoy

take tool/dmsetupm64e "$TOOLS/mips64el/dmsetup" "$VENTOY_PATH/DMSETUP/dmsetupm64e"
take tool/unsquashfs_m64e "$TOOLS/mips64el/unsquashfs" "$VENTOY_PATH/SQUASHFS/unsquashfs_m64e"
take tool/vtoy_fuse_iso_m64e "$TOOLS/mips64el/vtoy_fuse_iso" "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_m64e"
cp -a $VENTOY_PATH/VtoyTool/vtoytool tool/
rm -f tool/vtoytool/00/vtoytool_32
rm -f tool/vtoytool/00/vtoytool_64
rm -f tool/vtoytool/00/vtoytool_aa64
take tool/vtoytool/00/vtoytool_m64e "$TOOLS/mips64el/vtoytool" "$VENTOY_PATH/VtoyTool/vtoytool/00/vtoytool_m64e"
take tool/vblade_m64e "$TOOLS/mips64el/vblade" "$VENTOY_PATH/VBLADE/vblade-master/vblade_m64e"

take tool/lunzipm64e "$TOOLS/mips64el/lunzip" "$VENTOY_PATH/LZIP/lunzipm64e"
take tool/lz4catm64e "$TOOLS/mips64el/lz4cat" tool/lz4catm64e
take tool/zstdcatm64e "$TOOLS/mips64el/zstdcat" "$TOOLS/mips64el/zstdcat"

if [ -f "$INITRAMFS/mips64el/busybox.xz" ]; then
    cp -a "$INITRAMFS/mips64el/busybox.xz" busybox/busyboxm64e.xz
fi
if [ -f "$INITRAMFS/mips64el/vtchmod" ]; then
    cp -a "$INITRAMFS/mips64el/vtchmod" busybox/vtchmodm64e
fi

chmod -R 777 ./tool

find ./tool | cpio  -o -H newc --owner=root:root >tool.cpio
xz tool.cpio
rm -rf tool

cd ..
find .| cpio  -o -H newc --owner=root:root >../ventoy_mips64.cpio

cd ..
rm -rf cpio_tmp


########## cpio_loongarch64 ##############
rm -rf cpio_tmp
mkdir -p cpio_tmp/ventoy/tool cpio_tmp/ventoy/busybox
cp -a cpio_x86/ventoy/tool/*.sh cpio_tmp/ventoy/tool/

cd cpio_tmp/ventoy

take tool/dmsetupla64 "$TOOLS/loongarch64/dmsetup"
take tool/unsquashfs_la64 "$TOOLS/loongarch64/unsquashfs"
take tool/vtoy_fuse_iso_la64 "$TOOLS/loongarch64/vtoy_fuse_iso"
cp -a $VENTOY_PATH/VtoyTool/vtoytool tool/
rm -f tool/vtoytool/00/vtoytool_32
rm -f tool/vtoytool/00/vtoytool_64
rm -f tool/vtoytool/00/vtoytool_aa64
rm -f tool/vtoytool/00/vtoytool_m64e
take tool/vtoytool/00/vtoytool_la64 "$TOOLS/loongarch64/vtoytool"
take tool/vblade_la64 "$TOOLS/loongarch64/vblade"
take tool/lunzipla64 "$TOOLS/loongarch64/lunzip"
take tool/lz4catla64 "$TOOLS/loongarch64/lz4cat"
take tool/zstdcatla64 "$TOOLS/loongarch64/zstdcat"

if [ -f "$INITRAMFS/loongarch64/busybox.xz" ]; then
    cp -a "$INITRAMFS/loongarch64/busybox.xz" busybox/busyboxla64.xz
fi
# sbin/init is #!/ventoy/busybox/ash. GRUB renames this 3-letter stub to ash,
# the same way arm64 uses a64. The name has to stay 3 letters: the rename
# copies 18 bytes and leaves the existing NUL in place.
take busybox/l64 "$TOOLS/loongarch64/ash"
if [ -f "$INITRAMFS/loongarch64/vtchmod" ]; then
    cp -a "$INITRAMFS/loongarch64/vtchmod" busybox/vtchmodla64
fi
if [ -f "$TOOLS/loongarch64/xzminidec" ]; then
    cp -a "$TOOLS/loongarch64/xzminidec" busybox/xzminidecla64
fi

chmod -R 777 ./tool

find ./tool | cpio  -o -H newc --owner=root:root >tool.cpio
xz tool.cpio
rm -rf tool

cd ..
find .| cpio  -o -H newc --owner=root:root >../ventoy_loongarch64.cpio

cd ..
rm -rf cpio_tmp




echo '======== SUCCESS ============='

CPIO_DEST=${VTOY_CPIO_DEST:-$VENTOY_PATH/INSTALL/ventoy}
mkdir -p "$CPIO_DEST"
rm -f "$CPIO_DEST"/ventoy.cpio \
    "$CPIO_DEST"/ventoy_x86.cpio \
    "$CPIO_DEST"/ventoy_arm64.cpio \
    "$CPIO_DEST"/ventoy_mips64.cpio \
    "$CPIO_DEST"/ventoy_loongarch64.cpio
cp -a ventoy.cpio "$CPIO_DEST"/
cp -a ventoy_x86.cpio "$CPIO_DEST"/
cp -a ventoy_arm64.cpio "$CPIO_DEST"/
cp -a ventoy_mips64.cpio "$CPIO_DEST"/
cp -a ventoy_loongarch64.cpio "$CPIO_DEST"/

