#!/bin/bash

if [ "$1" = "CI" ]; then
    OPT='-dR'
else
    OPT='-a'
fi

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

if [ "$1" = "CI" ]; then
    sh ./download_ext.sh
fi

if [ ! -f ./EXT/vmlinuz ]; then
    echo "Please download EXT files firstly!"
    exit 1
fi

sh ./initrd.sh


rm -rf ISO_TMP
cp -a ISO ISO_TMP

if ! [ -d ISO_TMP ]; then
    echo "Copy ISO_TMP failed"
    exit 1
fi

cp -a ./EXT/vmlinuz ISO_TMP/EFI/boot/
mv ./initrd.img ISO_TMP/EFI/boot/initrd

cp -a GRUB/cdrom.img ISO_TMP/EFI/boot/
cp -a GRUB/bootx64.efi ISO_TMP/EFI/boot/


rm -rf efimnt
rm -f efi.img
mkdir -p efimnt

dd if=/dev/zero of=efi.img bs=1M count=2
mkfs.vfat efi.img
mount efi.img efimnt
mkdir -p efimnt/EFI/boot
cp $OPT GRUB/bootx64.efi efimnt/EFI/boot/
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

echo ""
if [ -f ventoy-${version}-livecd.iso ]; then
    echo "========== SUCCESS ============="
else
    echo "========== FAILED ============="
fi
echo ""


