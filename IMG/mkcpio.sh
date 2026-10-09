#!/bin/bash

VENTOY_PATH="$PWD/.."
EPOCH="${SOURCE_DATE_EPOCH:-1700000000}"

deterministic_cpio() {
    local target_dir="$1"
    local output_file="$2"
    chmod -R a+rX "$target_dir" 2>/dev/null || true
    find "$target_dir" -type f -exec chmod 755 {} + 2>/dev/null || true
    find "$target_dir" -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
    (
        cd "$target_dir"
        find . | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > "$output_file"
    )
    touch -h -d @"$EPOCH" "$output_file"
}

if [ -d cpio_tmp ]; then
    rm -rf cpio_tmp
fi

############### cpio ############
rm -f ventoy.cpio ventoy_x86.cpio ventoy_arm64.cpio ventoy_mips64.cpio

cp -a cpio cpio_tmp

cd cpio_tmp
chmod -R a+rX .
chmod 755 sbin/init 2>/dev/null || true
chmod 755 ventoy/init 2>/dev/null || true
find . -type f -name "*.sh" -exec chmod 755 {} + 2>/dev/null || true
rm -f init
ln -s sbin/init init
ln -s sbin/init linuxrc

cd ventoy

find ./loop -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
find ./loop | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > loop.cpio
touch -h -d @"$EPOCH" loop.cpio
xz -f loop.cpio
rm -rf loop

touch -h -d @"$EPOCH" ventoy_chain.sh ventoy_loop.sh
xz -f ventoy_chain.sh
xz -f ventoy_loop.sh

find ./hook -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
find ./hook | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > hook.cpio
touch -h -d @"$EPOCH" hook.cpio
xz -f hook.cpio
rm -rf hook
cd ..

find . -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
find . | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > ../ventoy.cpio
touch -h -d @"$EPOCH" ../ventoy.cpio

cd ..
rm -rf cpio_tmp

########## cpio_x86 ##############
cp -a cpio_x86 cpio_tmp

cd cpio_tmp/ventoy

[ -f "$VENTOY_PATH/DMSETUP/dmsetup32" ] && cp -a "$VENTOY_PATH/DMSETUP/dmsetup32" tool/
[ -f "$VENTOY_PATH/DMSETUP/dmsetup64" ] && cp -a "$VENTOY_PATH/DMSETUP/dmsetup64" tool/
[ -f "$VENTOY_PATH/SQUASHFS/unsquashfs_32" ] && cp -a "$VENTOY_PATH/SQUASHFS/unsquashfs_32" tool/
[ -f "$VENTOY_PATH/SQUASHFS/unsquashfs_64" ] && cp -a "$VENTOY_PATH/SQUASHFS/unsquashfs_64" tool/
[ -f "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_32" ] && cp -a "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_32" tool/
[ -f "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_64" ] && cp -a "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_64" tool/
if [ -d "$VENTOY_PATH/VtoyTool/vtoytool" ]; then
    cp -a "$VENTOY_PATH/VtoyTool/vtoytool" tool/
    rm -f tool/vtoytool/00/vtoytool_aa64 tool/vtoytool/00/vtoytool_m64e
fi
[ -f "$VENTOY_PATH/VBLADE/vblade-master/vblade_32" ] && cp -a "$VENTOY_PATH/VBLADE/vblade-master/vblade_32" tool/
[ -f "$VENTOY_PATH/VBLADE/vblade-master/vblade_64" ] && cp -a "$VENTOY_PATH/VBLADE/vblade-master/vblade_64" tool/
[ -f "$VENTOY_PATH/LZIP/lunzip32" ] && cp -a "$VENTOY_PATH/LZIP/lunzip32" tool/
[ -f "$VENTOY_PATH/LZIP/lunzip64" ] && cp -a "$VENTOY_PATH/LZIP/lunzip64" tool/
[ -f "$VENTOY_PATH/cryptsetup/veritysetup32" ] && cp -a "$VENTOY_PATH/cryptsetup/veritysetup32" tool/
[ -f "$VENTOY_PATH/cryptsetup/veritysetup64" ] && cp -a "$VENTOY_PATH/cryptsetup/veritysetup64" tool/

chmod -R a+rX tool
find tool -type f -exec chmod 755 {} + 2>/dev/null || true
find ./tool -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
find ./tool | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > tool.cpio
touch -h -d @"$EPOCH" tool.cpio
xz -f tool.cpio
rm -rf tool

    cd ..
    chmod -R a+rX .
    find . -type f -exec chmod 755 {} + 2>/dev/null || true
    find . -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
    find . | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > ../ventoy_x86.cpio
    touch -h -d @"$EPOCH" ../ventoy_x86.cpio

    cd ..
    rm -rf cpio_tmp

    ########## cpio_arm64 ##############
    cp -a cpio_arm64 cpio_tmp
    cp -a cpio_x86/ventoy/tool/*.sh cpio_tmp/ventoy/tool/ 2>/dev/null || true

    cd cpio_tmp/ventoy

    [ -f "$VENTOY_PATH/DMSETUP/dmsetupaa64" ] && cp -a "$VENTOY_PATH/DMSETUP/dmsetupaa64" tool/
    [ -f "$VENTOY_PATH/SQUASHFS/unsquashfs_aa64" ] && cp -a "$VENTOY_PATH/SQUASHFS/unsquashfs_aa64" tool/
    [ -f "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_aa64" ] && cp -a "$VENTOY_PATH/FUSEISO/vtoy_fuse_iso_aa64" tool/
    if [ -d "$VENTOY_PATH/VtoyTool/vtoytool" ]; then
        cp -a "$VENTOY_PATH/VtoyTool/vtoytool" tool/
        rm -f tool/vtoytool/00/vtoytool_32 tool/vtoytool/00/vtoytool_64 tool/vtoytool/00/vtoytool_m64e
    fi
    [ -f "$VENTOY_PATH/VBLADE/vblade-master/vblade_aa64" ] && cp -a "$VENTOY_PATH/VBLADE/vblade-master/vblade_aa64" tool/
    [ -f "$VENTOY_PATH/LZIP/lunzipaa64" ] && cp -a "$VENTOY_PATH/LZIP/lunzipaa64" tool/

    chmod -R a+rX tool
    find tool -type f -exec chmod 755 {} + 2>/dev/null || true
    find ./tool -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
    find ./tool | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > tool.cpio
    touch -h -d @"$EPOCH" tool.cpio
    xz -f tool.cpio
    rm -rf tool

    cd ..
    chmod -R a+rX .
    find . -type f -exec chmod 755 {} + 2>/dev/null || true
    find . -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
    find . | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > ../ventoy_arm64.cpio
    touch -h -d @"$EPOCH" ../ventoy_arm64.cpio

    cd ..
    rm -rf cpio_tmp

    ########## cpio_mips64 ##############
    cp -a cpio_mips64 cpio_tmp
    cp -a cpio_x86/ventoy/tool/*.sh cpio_tmp/ventoy/tool/ 2>/dev/null || true

    cd cpio_tmp/ventoy

    [ -f "$VENTOY_PATH/DMSETUP/dmsetupm64e" ] && cp -a "$VENTOY_PATH/DMSETUP/dmsetupm64e" tool/
    if [ -d "$VENTOY_PATH/VtoyTool/vtoytool" ]; then
        cp -a "$VENTOY_PATH/VtoyTool/vtoytool" tool/
        rm -f tool/vtoytool/00/vtoytool_32 tool/vtoytool/00/vtoytool_64 tool/vtoytool/00/vtoytool_aa64
    fi

    chmod -R a+rX tool
    find tool -type f -exec chmod 755 {} + 2>/dev/null || true
    find ./tool -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
    find ./tool | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > tool.cpio
    touch -h -d @"$EPOCH" tool.cpio
    xz -f tool.cpio
    rm -rf tool

    cd ..
    chmod -R a+rX .
    find . -type f -exec chmod 755 {} + 2>/dev/null || true
    find . -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
    find . | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > ../ventoy_mips64.cpio
touch -h -d @"$EPOCH" ../ventoy_mips64.cpio

cd ..
rm -rf cpio_tmp

echo '======== SUCCESS ============='

mkdir -p "$VENTOY_PATH/INSTALL/ventoy"
cp -a ventoy.cpio "$VENTOY_PATH/INSTALL/ventoy/"
cp -a ventoy_x86.cpio "$VENTOY_PATH/INSTALL/ventoy/"
cp -a ventoy_arm64.cpio "$VENTOY_PATH/INSTALL/ventoy/"
cp -a ventoy_mips64.cpio "$VENTOY_PATH/INSTALL/ventoy/"
