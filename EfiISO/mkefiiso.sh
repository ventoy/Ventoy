#!/bin/sh

# Remove previous Ventoy EFI boot image
rm -f ventoy_efiboot.img.*

# Create new ISO in the ISO directory
(
    cd ISO || exit 1
    mkisofs -R -D -sysid VENTOY -V VENTOY \
            -P "longpanda admin@ventoy.net" \
            -p 'https://www.ventoy.net' \
            -o ../ventoy_efiboot.img ./ 
)

# Compress the new EFI boot image
xz --check=crc32 ventoy_efiboot.img

# Replace the existing compressed EFI boot image in the INSTALL directory
rm -f ../INSTALL/ventoy/ventoy_efiboot.img.xz
cp -a ventoy_efiboot.img.xz ../INSTALL/ventoy/
