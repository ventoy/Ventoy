#!/bin/bash

date +"%Y/%m/%d %H:%M:%S"
echo downloading EXT files ...

wget -q -P ./EXT/ https://github.com/ventoy/KioskFiles/releases/download/v1.0/Porteus-Kiosk-5.2.0-x86_64.iso
wget -q -P ./EXT/ https://github.com/ventoy/KioskFiles/releases/download/v1.0/06-fonts.xzm

[ -d ./__tmp__ ] && rm -rf ./__tmp__
mkdir __tmp__

mount ./EXT/Porteus-Kiosk-5.2.0-x86_64.iso ./__tmp__
cp -a ./__tmp__/boot/vmlinuz   ./EXT/
cp -a ./__tmp__/boot/initrd.xz ./EXT/
cp -a ./__tmp__/xzm/* ./EXT/

umount ./__tmp__
rm -rf ./__tmp__
rm -f ./EXT/Porteus-Kiosk-5.2.0-x86_64.iso

date +"%Y/%m/%d %H:%M:%S"
echo downloading EXT files finish ...

