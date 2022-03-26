#!/bin/bash

VENTOY_PATH=$PWD/../


if [ -d cpio_tmp ]; then
    rm -rf cpio_tmp
fi


############### cpio ############
chmod -R 777 cpio
rm -f ventoy.cpio ventoy_x86.cpio ventoy_arm64.cpio ventoy_mips64.cpio 

cp -a cpio	cpio_tmp

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
chmod -R 777 cpio_x86
cp -a cpio_x86	cpio_tmp

cd cpio_tmp/ventoy

cp -a $VENTOY_PATH/DMSETUP/dmsetup32 tool/
cp -a $VENTOY_PATH/DMSETUP/dmsetup64 tool/
cp -a $VENTOY_PATH/SQUASHFS/unsquashfs_32 tool/
cp -a $VENTOY_PATH/SQUASHFS/unsquashfs_64 tool/
cp -a $VENTOY_PATH/FUSEISO/vtoy_fuse_iso_32 tool/
cp -a $VENTOY_PATH/FUSEISO/vtoy_fuse_iso_64 tool/
cp -a $VENTOY_PATH/VtoyTool/vtoytool tool/
rm -f tool/vtoytool/00/vtoytool_aa64
rm -f tool/vtoytool/00/vtoytool_m64e
cp -a $VENTOY_PATH/VBLADE/vblade-master/vblade_32 tool/
cp -a $VENTOY_PATH/VBLADE/vblade-master/vblade_64 tool/

cp -a $VENTOY_PATH/LZIP/lunzip32 tool/
cp -a $VENTOY_PATH/LZIP/lunzip64 tool/

cp -a $VENTOY_PATH/cryptsetup/veritysetup32 tool/
cp -a $VENTOY_PATH/cryptsetup/veritysetup64 tool/

chmod -R 777 ./tool

find ./tool | cpio  -o -H newc --owner=root:root >tool.cpio
xz tool.cpio
rm -rf tool

cd ..
find .| cpio  -o -H newc --owner=root:root >../ventoy_x86.cpio

cd ..
rm -rf cpio_tmp


########## cpio_arm64 ##############
chmod -R 777 cpio_arm64
cp -a cpio_arm64	cpio_tmp
cp -a cpio_x86/ventoy/tool/*.sh cpio_tmp/ventoy/tool/

cd cpio_tmp/ventoy

cp -a $VENTOY_PATH/DMSETUP/dmsetupaa64 tool/
cp -a $VENTOY_PATH/SQUASHFS/unsquashfs_aa64 tool/
cp -a $VENTOY_PATH/FUSEISO/vtoy_fuse_iso_aa64 tool/
cp -a $VENTOY_PATH/VtoyTool/vtoytool tool/
rm -f tool/vtoytool/00/vtoytool_32
rm -f tool/vtoytool/00/vtoytool_64
rm -f tool/vtoytool/00/vtoytool_m64e
cp -a $VENTOY_PATH/VBLADE/vblade-master/vblade_aa64 tool/

cp -a $VENTOY_PATH/LZIP/lunzipaa64 tool/

chmod -R 777 ./tool

find ./tool | cpio  -o -H newc --owner=root:root >tool.cpio
xz tool.cpio
rm -rf tool

cd ..
find .| cpio  -o -H newc --owner=root:root >../ventoy_arm64.cpio

cd ..
rm -rf cpio_tmp



########## cpio_mips64 ##############
chmod -R 777 cpio_mips64
cp -a cpio_mips64	cpio_tmp
cp -a cpio_x86/ventoy/tool/*.sh cpio_tmp/ventoy/tool/

cd cpio_tmp/ventoy

cp -a $VENTOY_PATH/DMSETUP/dmsetupm64e tool/
# cp -a $VENTOY_PATH/SQUASHFS/unsquashfs_m64e tool/
# cp -a $VENTOY_PATH/FUSEISO/vtoy_fuse_iso_m64e tool/
cp -a $VENTOY_PATH/VtoyTool/vtoytool tool/
rm -f tool/vtoytool/00/vtoytool_32
rm -f tool/vtoytool/00/vtoytool_64
rm -f tool/vtoytool/00/vtoytool_aa64
# cp -a $VENTOY_PATH/VBLADE/vblade-master/vblade_m64e tool/

# cp -a $VENTOY_PATH/LZIP/lunzipaa64 tool/

chmod -R 777 ./tool

find ./tool | cpio  -o -H newc --owner=root:root >tool.cpio
xz tool.cpio
rm -rf tool

cd ..
find .| cpio  -o -H newc --owner=root:root >../ventoy_mips64.cpio

cd ..
rm -rf cpio_tmp




echo '======== SUCCESS ============='

rm -f $VENTOY_PATH/INSTALL/ventoy/ventoy.cpio
rm -f $VENTOY_PATH/INSTALL/ventoy/ventoy_x86.cpio
rm -f $VENTOY_PATH/INSTALL/ventoy/ventoy_arm64.cpio
rm -f $VENTOY_PATH/INSTALL/ventoy/ventoy_mips64.cpio
cp -a ventoy.cpio $VENTOY_PATH/INSTALL/ventoy/
cp -a ventoy_x86.cpio $VENTOY_PATH/INSTALL/ventoy/
cp -a ventoy_arm64.cpio $VENTOY_PATH/INSTALL/ventoy/
cp -a ventoy_mips64.cpio $VENTOY_PATH/INSTALL/ventoy/

