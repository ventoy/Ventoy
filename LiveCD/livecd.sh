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

rm -rf ISO_TMP
cp -a ISO ISO_TMP

cp -a VTOY VTOY_TMP && cd VTOY_TMP
gcc -O2 -m32 ./ventoy/disksize.c -o ./ventoy/disksize
rm -f ./ventoy/disksize.c
find . | cpio  -o -H newc | gzip -9 > ../ISO_TMP/EFI/ventoy/ventoy.gz
cd .. && rm -rf VTOY_TMP


cp -a $VENTOY_PATH/INSTALL/ventoy-${version}-linux.tar.gz ISO_TMP/EFI/ventoy/
cp -a GRUB/cdrom.img ISO_TMP/EFI/boot/
cp -a GRUB/bootx64.efi ISO_TMP/EFI/boot/


rm -rf efimnt
rm -f efi.img
mkdir -p efimnt

dd if=/dev/zero of=efi.img bs=1M count=2
mkfs.vfat efi.img
mount efi.img efimnt
mkdir -p efimnt/EFI/boot
cp -a GRUB/bootx64.efi efimnt/EFI/boot/
umount efimnt

sync
cp -a efi.img ISO_TMP/EFI/boot/

rm -rf efimnt
rm -f efi.img


cd ISO_TMP

sed "s/xxx/$version/g" -i EFI/boot/grub.cfg

rm -f ../ventoy-${version}-livecd.iso

xorriso -as mkisofs  -allow-lowercase  --sort-weight 0 / --sort-weight 1 /EFI  -v -R -J  -V  'VentoyLiveCD' -P 'VENTOY COMPATIBLE' -p 'https://www.ventoy.net' -sysid 'Ventoy' -A 'VentoyLiveCD' -b EFI/boot/cdrom.img --grub2-boot-info --grub2-mbr ../GRUB/boot_hybrid.img  -c EFI/boot/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table -eltorito-alt-boot -e EFI/boot/efi.img -no-emul-boot  -append_partition 2 0xEF  EFI/boot/efi.img   -o ../ventoy-${version}-livecd.iso  .

cd ../
rm -rf ISO_TMP

