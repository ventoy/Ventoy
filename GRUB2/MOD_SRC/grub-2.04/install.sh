#!/bin/bash

VT_DIR=$PWD/../../..

rm -rf $VT_DIR/GRUB2/INSTALL
rm -rf $VT_DIR/GRUB2/PXE
mkdir -p $VT_DIR/GRUB2/INSTALL
mkdir -p $VT_DIR/GRUB2/PXE

make install

PATH=$PATH:$VT_DIR/GRUB2/INSTALL/bin/:$VT_DIR/GRUB2/INSTALL/sbin/

net_modules_legacy="net tftp http"
all_modules_legacy="date drivemap blocklist ntldr search at_keyboard usb_keyboard  gcry_md5 hashsum gzio xzio lzopio lspci pci ext2 xfs ventoy chain read halt iso9660 linux16 test true sleep reboot echo videotest videoinfo videotest_checksum video_colors video_cirrus video_bochs vga vbe video_fb font video gettext extcmd terminal  linux minicmd help configfile tr trig boot biosdisk disk ls tar squash4 password_pbkdf2 all_video png jpeg part_gpt part_msdos fat exfat ntfs loopback gzio normal  udf gfxmenu gfxterm gfxterm_background gfxterm_menu"

net_modules_uefi="efinet net tftp http"
all_modules_uefi="blocklist ventoy test search at_keyboard usb_keyboard  gcry_md5 hashsum gzio xzio lzopio ext2 xfs read halt sleep serial terminfo png password_pbkdf2 gcry_sha512 pbkdf2 part_gpt part_msdos ls tar squash4 loopback part_apple minicmd diskfilter linux relocator jpeg iso9660 udf hfsplus halt acpi mmap gfxmenu video_colors trig bitmap_scale gfxterm bitmap font fat exfat ntfs fshelp efifwsetup reboot echo configfile normal terminal gettext chain  priority_queue bufio datetime cat extcmd crypto gzio boot all_video efi_gop efi_uga video_bochs video_cirrus video video_fb gfxterm_background gfxterm_menu"

all_extra_modules="elf macho offsetio regexp file"

if [ "$1" = "uefi" ]; then
    all_modules="$net_modules_uefi $all_modules_uefi $all_extra_modules"
    grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/x86_64-efi" --prefix '(,msdos2)/grub' --output "$VT_DIR/INSTALL/EFI/BOOT/grubx64_real.efi"  --format 'x86_64-efi' --compression 'auto'  $all_modules_uefi 'fat' 'part_msdos'
else
    all_modules="$net_modules_legacy $all_modules_legacy"
    grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc" --prefix '(,msdos2)/grub' --output "$VT_DIR/INSTALL/grub/i386-pc/core.img"  --format 'i386-pc' --compression 'auto'  $all_modules_legacy  'fat' 'part_msdos' 'biosdisk' 
fi

grub-mknetdir  --modules="$all_modules" --net-directory=$VT_DIR/GRUB2/PXE  --subdir=grub2 --locales=en@quot || exit 1

if [ "$1" = "uefi" ]; then
    rm -f $VT_DIR/GRUB2/NBP/core.efi
    cp -a $VT_DIR/GRUB2/PXE/grub2/x86_64-efi/core.efi  $VT_DIR/GRUB2/NBP/core.efi || exit 1
    
    rm -f $VT_DIR/INSTALL/grub/x86_64-efi/normal.mod
    cp -a $VT_DIR/GRUB2/PXE/grub2/x86_64-efi/normal.mod    $VT_DIR/INSTALL/grub/x86_64-efi/normal.mod  || exit 1
else
    rm -f $VT_DIR/GRUB2/NBP/core.0
    cp -a $VT_DIR/GRUB2/PXE/grub2/i386-pc/core.0    $VT_DIR/GRUB2/NBP/core.0  || exit 1
    
    for md in $all_extra_modules; do
        rm -f $VT_DIR/INSTALL/grub/i386-pc/${md}.mod
        cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc/${md}.mod  $VT_DIR/INSTALL/grub/i386-pc/
    done
    
    rm -f $VT_DIR/INSTALL/grub/i386-pc/boot.img
    cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc/boot.img  $VT_DIR/INSTALL/grub/i386-pc/boot.img   || exit 1
fi
