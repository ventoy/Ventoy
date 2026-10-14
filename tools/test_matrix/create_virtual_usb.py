#!/usr/bin/env python3
import os
import struct
import sys

WORKSPACE = os.environ.get("VTOY_WORKSPACE", "/ventoy" if os.path.exists("/ventoy") else os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
IMAGE_PATH = os.path.join(WORKSPACE, "ventoy_usb.img")
TOTAL_SIZE_MB = 512
SECTOR_SIZE = 512
TOTAL_SECTORS = (TOTAL_SIZE_MB * 1024 * 1024) // SECTOR_SIZE

PART1_START = 2048
PART1_SIZE = 978944
PART1_TYPE = 0x07

# Ventoy specification requires partition 2 (VTOYEFI) to be exactly 65536 sectors (32MB) of type 0xEF.
PART2_START = PART1_START + PART1_SIZE
PART2_SIZE = 65536
PART2_TYPE = 0xEF

BOOT_IMG_PATH = os.path.join(WORKSPACE, "INSTALL", "grub", "i386-pc", "boot.img")
CORE_IMG_PATH = os.path.join(WORKSPACE, "INSTALL", "grub", "i386-pc", "core.img")

def make_mbr_sector():
    mbr = bytearray(512)

    if os.path.exists(BOOT_IMG_PATH):
        with open(BOOT_IMG_PATH, "rb") as f:
            boot_code = f.read(446)
            mbr[0:len(boot_code)] = boot_code

    struct.pack_into("<I", mbr, 0x1B8, 0x20202020)
    struct.pack_into("<B3sB3sII", mbr, 0x1BE, 0x00, b'\x00\x02\x00', PART1_TYPE, b'\xff\xff\xff', PART1_START, PART1_SIZE)
    struct.pack_into("<B3sB3sII", mbr, 0x1CE, 0x80, b'\x00\x02\x00', PART2_TYPE, b'\xff\xff\xff', PART2_START, PART2_SIZE)

    mbr[510] = 0x55
    mbr[511] = 0xAA

    return mbr

def main():
    with open(IMAGE_PATH, "wb") as f:
        f.write(make_mbr_sector())

        # BIOS boot embeds GRUB core.img in the post-MBR gap starting at sector 1.
        if os.path.exists(CORE_IMG_PATH):
            with open(CORE_IMG_PATH, "rb") as cf:
                f.write(cf.read())

        f.seek(TOTAL_SECTORS * SECTOR_SIZE - 1)
        f.write(b'\x00')

if __name__ == "__main__":
    main()
