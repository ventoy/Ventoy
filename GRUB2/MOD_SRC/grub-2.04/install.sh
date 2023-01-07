#!/bin/bash

VT_DIR=$PWD/../../..

rm -rf $VT_DIR/GRUB2/INSTALL
rm -rf $VT_DIR/GRUB2/PXE
mkdir -p $VT_DIR/GRUB2/INSTALL
mkdir -p $VT_DIR/GRUB2/PXE

make install

PATH=$VT_DIR/GRUB2/INSTALL/bin/:$VT_DIR/GRUB2/INSTALL/sbin/:$PATH

net_modules_legacy="net tftp http"
all_modules_legacy="file date drivemap blocklist newc vga_text ntldr search at_keyboard usb_keyboard  gcry_md5 hashsum gzio xzio lzopio lspci pci ext2 xfs ventoy chain read halt iso9660 linux16 test true sleep reboot echo videotest videoinfo videotest_checksum video_colors video_cirrus video_bochs vga vbe font video gettext extcmd terminal  linux minicmd help configfile tr trig boot biosdisk disk ls tar squash4 password_pbkdf2 all_video png jpeg part_gpt part_msdos fat exfat ntfs loopback gzio normal video_fb udf gfxmenu gfxterm gfxterm_background gfxterm_menu smbios"

net_modules_uefi="efinet net tftp http"
all_modules_uefi="file setkey blocklist ventoy test true regexp newc search at_keyboard usb_keyboard  gcry_md5 hashsum gzio xzio lzopio ext2 xfs read halt sleep serial terminfo png password_pbkdf2 gcry_sha512 pbkdf2 part_gpt part_msdos ls tar squash4 loopback part_apple minicmd diskfilter linux relocator jpeg iso9660 udf hfsplus halt acpi mmap gfxmenu video_colors trig bitmap_scale gfxterm bitmap font fat exfat ntfs fshelp efifwsetup reboot echo configfile normal terminal gettext chain  priority_queue bufio datetime cat extcmd crypto gzio boot all_video efi_gop efi_uga video_bochs video_cirrus video video_fb gfxterm_background gfxterm_menu mouse fwload smbios zfs"

all_modules_arm64_uefi="file setkey blocklist ventoy test true regexp newc search  gcry_md5 hashsum gzio xzio lzopio ext2 xfs read halt sleep serial terminfo png password_pbkdf2 gcry_sha512 pbkdf2 part_gpt part_msdos ls tar squash4 loopback part_apple minicmd diskfilter linux jpeg iso9660 udf hfsplus halt acpi mmap gfxmenu video_colors trig bitmap_scale gfxterm bitmap font fat exfat ntfs fshelp efifwsetup reboot echo configfile normal terminal gettext chain  priority_queue bufio datetime cat extcmd crypto gzio boot all_video efi_gop video video_fb gfxterm_background gfxterm_menu zfs"

all_modules_mips64el_uefi="file setkey blocklist ventoy test true regexp newc search  gcry_md5 hashsum gzio xzio lzopio ext2 xfs read halt sleep serial terminfo png password_pbkdf2 gcry_sha512 pbkdf2 part_gpt part_msdos ls tar squash4 loopback part_apple minicmd diskfilter linux jpeg iso9660 udf hfsplus halt acpi mmap gfxmenu video_colors trig bitmap_scale gfxterm bitmap font fat exfat ntfs fshelp efifwsetup reboot echo configfile normal terminal gettext chain  priority_queue bufio datetime cat extcmd crypto gzio boot all_video efi_gop video video_fb gfxterm_background gfxterm_menu zfs"


if [ "$1" = "uefi" ]; then
    all_modules="$net_modules_uefi $all_modules_uefi "

    grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/x86_64-efi" --prefix '(,2)/grub' --output "$VT_DIR/INSTALL/EFI/BOOT/grubx64_real.efi"  --format 'x86_64-efi' --compression 'auto'  $all_modules_uefi

    #grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/x86_64-efi" -c "$VT_DIR/LiveCD/GRUB/embed.cfg" --prefix '/EFI/boot' --output "$VT_DIR/LiveCD/GRUB/bootx64.efi"  --format 'x86_64-efi' --compression 'auto'  $all_modules_uefi 'fat' 'part_msdos'
elif [ "$1" = "i386efi" ]; then
    all_modules="$net_modules_uefi $all_modules_uefi "

    grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/i386-efi" --prefix '(,2)/grub' --output "$VT_DIR/INSTALL/EFI/BOOT/grubia32_real.efi"  --format 'i386-efi' --compression 'auto'  $all_modules_uefi
elif [ "$1" = "arm64" ]; then
    all_modules="$net_modules_uefi $all_modules_arm64_uefi "

    grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/arm64-efi" --prefix '(,2)/grub' --output "$VT_DIR/INSTALL/EFI/BOOT/BOOTAA64.EFI"  --format 'arm64-efi' --compression 'auto'  $all_modules_arm64_uefi
elif [ "$1" = "mips64el" ]; then
    all_modules="$net_modules_uefi $all_modules_mips64el_uefi "

    grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/mips64el-efi" --prefix '(,2)/grub' --output "$VT_DIR/INSTALL/EFI/BOOT/BOOTMIPS.EFI"  --format 'mips64el-efi' --compression 'auto'  $all_modules_mips64el_uefi
else
    all_modules="$net_modules_legacy $all_modules_legacy "
    grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc" --prefix '(,2)/grub' --output "$VT_DIR/INSTALL/grub/i386-pc/core.img"  --format 'i386-pc' --compression 'auto'  $all_modules_legacy  'fat' 'part_msdos' 'biosdisk' 
    
    #grub-mkimage -v --directory "$VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc" -c "$VT_DIR/LiveCD/GRUB/embed.cfg" --prefix '/EFI/boot' --output "$VT_DIR/LiveCD/GRUB/cdrom.img"  --format 'i386-pc-eltorito' --compression 'auto'  $all_modules_legacy 'biosdisk' 'iso9660' 'fat' 'part_msdos'
    #rm -f $VT_DIR/LiveCD/GRUB/boot_hybrid.img
    #cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc/boot_hybrid.img  $VT_DIR/LiveCD/GRUB/boot_hybrid.img
fi

grub-mknetdir  --modules="$all_modules" --net-directory=$VT_DIR/GRUB2/PXE  --subdir=grub2 --locales=en@quot || exit 1

if [ "$1" = "uefi" ]; then
    rm -f $VT_DIR/GRUB2/NBP/core.efi
    cp -a $VT_DIR/GRUB2/PXE/grub2/x86_64-efi/core.efi  $VT_DIR/GRUB2/NBP/core.efi || exit 1
    
    rm -rf $VT_DIR/INSTALL/grub/x86_64-efi
    mkdir -p $VT_DIR/INSTALL/grub/x86_64-efi
        
    cp -a $VT_DIR/GRUB2/PXE/grub2/x86_64-efi/normal.mod    $VT_DIR/INSTALL/grub/x86_64-efi/normal.mod  || exit 1      

    #copy other modules
    ls -1 $VT_DIR/GRUB2/INSTALL/lib/grub/x86_64-efi/ | egrep '\.(lst|mod)$' | while read line; do
        if ! echo $all_modules | grep -q " ${line%.mod} "; then
            echo "Copy $line ..."
            cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/x86_64-efi/$line    $VT_DIR/INSTALL/grub/x86_64-efi/
        fi
    done
elif [ "$1" = "i386efi" ]; then
    rm -f $VT_DIR/GRUB2/NBP/core.efi
    cp -a $VT_DIR/GRUB2/PXE/grub2/i386-efi/core.efi  $VT_DIR/GRUB2/NBP/core.efi || exit 1
    
    rm -rf $VT_DIR/INSTALL/grub/i386-efi
    mkdir -p $VT_DIR/INSTALL/grub/i386-efi

    cp -a $VT_DIR/GRUB2/PXE/grub2/i386-efi/normal.mod    $VT_DIR/INSTALL/grub/i386-efi/normal.mod  || exit 1      

    #copy other modules
    ls -1 $VT_DIR/GRUB2/INSTALL/lib/grub/i386-efi/ | egrep '\.(lst|mod)$' | while read line; do
        if ! echo $all_modules | grep -q " ${line%.mod} "; then
            echo "Copy $line ..."
            cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/i386-efi/$line    $VT_DIR/INSTALL/grub/i386-efi/
        fi
    done
elif [ "$1" = "arm64" ]; then
    rm -f $VT_DIR/GRUB2/NBP/core.efi
    cp -a $VT_DIR/GRUB2/PXE/grub2/arm64-efi/core.efi  $VT_DIR/GRUB2/NBP/core.efi || exit 1
    
    rm -rf $VT_DIR/INSTALL/grub/arm64-efi
    mkdir -p $VT_DIR/INSTALL/grub/arm64-efi

    cp -a $VT_DIR/GRUB2/PXE/grub2/arm64-efi/normal.mod    $VT_DIR/INSTALL/grub/arm64-efi/normal.mod  || exit 1      

    #copy other modules
    ls -1 $VT_DIR/GRUB2/INSTALL/lib/grub/arm64-efi/ | egrep '\.(lst|mod)$' | while read line; do
        if ! echo $all_modules | grep -q " ${line%.mod} "; then
            echo "Copy $line ..."
            cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/arm64-efi/$line    $VT_DIR/INSTALL/grub/arm64-efi/
            xz $VT_DIR/INSTALL/grub/arm64-efi/$line
            mv $VT_DIR/INSTALL/grub/arm64-efi/${line}.xz $VT_DIR/INSTALL/grub/arm64-efi/${line}
        fi
    done
elif [ "$1" = "mips64el" ]; then
    rm -f $VT_DIR/GRUB2/NBP/core.efi
    cp -a $VT_DIR/GRUB2/PXE/grub2/mips64el-efi/core.efi  $VT_DIR/GRUB2/NBP/core.efi || exit 1
    
    rm -rf $VT_DIR/INSTALL/grub/mips64el-efi
    mkdir -p $VT_DIR/INSTALL/grub/mips64el-efi

    cp -a $VT_DIR/GRUB2/PXE/grub2/mips64el-efi/normal.mod    $VT_DIR/INSTALL/grub/mips64el-efi/normal.mod  || exit 1      

    #copy other modules
    ls -1 $VT_DIR/GRUB2/INSTALL/lib/grub/mips64el-efi/ | egrep '\.(lst|mod)$' | while read line; do
        if ! echo $all_modules | grep -q " ${line%.mod} "; then
            echo "Copy $line ..."
            cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/mips64el-efi/$line    $VT_DIR/INSTALL/grub/mips64el-efi/
            xz $VT_DIR/INSTALL/grub/mips64el-efi/$line
            mv $VT_DIR/INSTALL/grub/mips64el-efi/${line}.xz $VT_DIR/INSTALL/grub/mips64el-efi/${line}
        fi
    done
else
    rm -f $VT_DIR/GRUB2/NBP/core.0
    cp -a $VT_DIR/GRUB2/PXE/grub2/i386-pc/core.0    $VT_DIR/GRUB2/NBP/core.0  || exit 1
    
    rm -f $VT_DIR/INSTALL/grub/i386-pc/boot.img
    cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc/boot.img  $VT_DIR/INSTALL/grub/i386-pc/boot.img   || exit 1
    
    #copy other modules
    ls -1 $VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc/ | egrep '\.(lst|mod)$' | while read line; do
        if ! echo $all_modules | grep -q " ${line%.mod} "; then
            echo "Copy $line ..."
            rm -f $VT_DIR/INSTALL/grub/i386-pc/$line
            cp -a $VT_DIR/GRUB2/INSTALL/lib/grub/i386-pc/$line    $VT_DIR/INSTALL/grub/i386-pc/
        fi
    done
fi
