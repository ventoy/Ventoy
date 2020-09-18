#!/bin/bash

VENTOY_PATH=$PWD/../

rm -f ventoy.cpio

chmod -R 777 cpio

cp -a cpio	cpio_tmp

cd cpio_tmp
rm -f init
ln -s sbin/init init
ln -s sbin/init linuxrc

cd ventoy


cp -a $VENTOY_PATH/DMSETUP/dmsetup* tool/
cp -a $VENTOY_PATH/SQUASHFS/unsquashfs_* tool/
cp -a $VENTOY_PATH/FUSEISO/vtoy_fuse_iso_* tool/
cp -a $VENTOY_PATH/VtoyTool/vtoytool tool/
cp -a $VENTOY_PATH/VBLADE/vblade-master/vblade_* tool/
cp -a $VENTOY_PATH/LZIP/lunzip32 tool/
cp -a $VENTOY_PATH/LZIP/lunzip64 tool/


chmod -R 777 ./tool

find ./tool | cpio  -o -H newc>tool.cpio
xz tool.cpio
rm -rf tool

find ./loop | cpio  -o -H newc>loop.cpio
xz loop.cpio
rm -rf loop

xz ventoy_chain.sh
xz ventoy_loop.sh

find ./hook | cpio  -o -H newc>hook.cpio
xz hook.cpio
rm -rf hook
cd ..

find .| cpio  -o -H newc>../ventoy.cpio

cd ..
rm -rf cpio_tmp

echo '======== SUCCESS ============='

rm -f $VENTOY_PATH/INSTALL/ventoy/ventoy.cpio
cp -a ventoy.cpio $VENTOY_PATH/INSTALL/ventoy/

