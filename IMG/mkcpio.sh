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



find ./tool | cpio  -o -H newc>tool.cpio
xz tool.cpio
rm -rf tool

xz ventoy.sh

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

