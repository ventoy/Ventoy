#!/bin/bash

VENTOY_PATH=$PWD/../

if ! [ -f $VENTOY_PATH/INSTALL/grub/grub.cfg ]; then
    echo "no grub.cfg detected"
    exit 1
fi

version=$(grep 'set.*VENTOY_VERSION=' $VENTOY_PATH/INSTALL/grub/grub.cfg | awk -F'"' '{print $2}')

if ! [ -e $VENTOY_PATH/INSTALL/ventoy-${version}-linux.tar.gz ]; then
    echo "no ventoy-${version}-linux.tar.gz detected"
    exit 1
fi

echo "Ventoy LiveCD GUI $version"

[ -d _INITRD_ ] && rm -rf _INITRD_
mkdir _INITRD_
cd _INITRD_

xzcat ../EXT/initrd.xz | cpio -idmu --quiet >/dev/null 2>&1
cp -a ../EXT/*.xzm ./
cp -a ../VTOY ./
cp -a ../EXT/ntfs-3g.tar.gz ./VTOY/
cp -a ../EXT/busybox-x86_64 ./VTOY/busybox

chown -R 0:0 ./VTOY
chmod -R 777 ./VTOY


mkdir ventoy
tar -xf $VENTOY_PATH/INSTALL/ventoy-${version}-linux.tar.gz -C .
mv ./ventoy-${version}  ./ventoy/ventoy
chmod -R 777 ./ventoy
mksquashfs ventoy ventoy.xzm  -comp xz 
rm -rf ./ventoy


rm -f ../initrd.img
find . | cpio --quiet -o -H newc  > ../initrd.img

cd ..
rm -rf _INITRD_

