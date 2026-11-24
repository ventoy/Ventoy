#!/bin/sh
# Assemble a Ventoy disk image from build/out and the INSTALL payload.
# No root and no loop device: partitions are written with sfdisk and mtools.
# Usage: build/pack-disk.sh
# Output: build/out/ventoy.img

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/build/grub-module-sets.sh"
OUT="$ROOT/build/out"
WORK="$ROOT/build/work/disk"
STAGE="$WORK/stage"
IMG="$OUT/ventoy.img"

GAP=2048
# 3 GiB exFAT for the ISO files. Three Debian netinst images are about 2.2 GiB.
DATA_SECTORS=$((3 * 1024 * 2048))
# 64 MiB ESP, the same size the installer writes.
ESP_SECTORS=131072
P1=$GAP
P2=$((GAP + DATA_SECTORS))

rm -rf "$STAGE"
mkdir -p "$STAGE/EFI/BOOT" "$STAGE/grub" "$STAGE/ventoy" "$WORK"

cp -a "$ROOT/INSTALL/grub/." "$STAGE/grub/"

copy_mods() {
    plat=$1
    src="$OUT/prefix-$plat/lib/grub/$plat"
    dest="$STAGE/grub/$plat"
    if [ ! -d "$src" ]; then
        echo "pack-disk: no $plat modules, skipping"
        return 0
    fi
    grub_install_esp_mods "$plat" "$src" "$dest"
}

for plat in x86_64-efi i386-efi i386-pc arm64-efi loongarch64-efi mips64el-efi; do
    copy_mods "$plat"
done

copy_efi() {
    name=$1
    src=$2
    if [ -f "$src" ]; then
        cp -a "$src" "$STAGE/EFI/BOOT/$name"
    else
        echo "pack-disk: no $name"
    fi
}

stage_bytes() {
    du -sb "$STAGE" | awk '{print $1}'
}

# BOOTX64.EFI is the signed Rocky shim. GRUB is grubx64_real.efi.
copy_efi BOOTX64.EFI "$ROOT/INSTALL/EFI/BOOT/BOOTX64.EFI"
copy_efi mmx64.efi "$ROOT/INSTALL/EFI/BOOT/mmx64.efi"
copy_efi fbx64.efi "$ROOT/INSTALL/EFI/BOOT/fbx64.efi"
copy_efi grubx64_real.efi "$ROOT/INSTALL/EFI/BOOT/grubx64_real.efi"
# BOOTIA32.EFI is the Super-UEFIinSecureBoot-Disk shim. The shim loads
# grubia32.efi, which chainloads grubia32_real.efi.
copy_efi BOOTIA32.EFI "$ROOT/INSTALL/EFI/BOOT/BOOTIA32.EFI"
copy_efi grubia32.efi "$ROOT/INSTALL/EFI/BOOT/grubia32.efi"
copy_efi mmia32.efi "$ROOT/INSTALL/EFI/BOOT/mmia32.efi"
copy_efi grubia32_real.efi "$ROOT/INSTALL/EFI/BOOT/grubia32_real.efi"

# Payload that is still shipped as data (Windows boot files, memdisk, ipxe).
# EFI applications built in this tree replace the matching prebuilt files.
find "$ROOT/INSTALL/ventoy" -maxdepth 1 -type f -exec cp -a {} "$STAGE/ventoy/" \;

place_efi() {
    dest=$1
    src=$2
    if [ -f "$src" ]; then
        cp -a "$src" "$STAGE/ventoy/$dest"
    fi
}

# Ventoy.efi is rebuilt for every architecture so a 64 MiB ESP is accepted.
# The other EFI payloads stay the prebuilt files copied from INSTALL/ventoy.
place_efi ventoy_x64.efi "$OUT/edk2/X64/Ventoy.efi"
place_efi ventoy_ia32.efi "$OUT/edk2/IA32/Ventoy.efi"
place_efi ventoy_aa64.efi "$OUT/edk2/AARCH64/Ventoy.efi"
place_efi ventoy_loongarch64.efi "$OUT/edk2/LOONGARCH64/Ventoy.efi"
place_efi vtoyutil_loongarch64.efi "$OUT/edk2/LOONGARCH64/VtoyUtil.efi"
place_efi iso9660_loongarch64.efi "$OUT/efifs/iso9660_loongarch64.efi"
place_efi udf_loongarch64.efi "$OUT/efifs/udf_loongarch64.efi"

(
    cd "$ROOT/IMG"
    VTOY_CPIO_DEST="$OUT/cpio" bash mkcpio.sh
)
find "$OUT/cpio" -maxdepth 1 -type f -name 'ventoy*.cpio' -exec cp -a {} "$STAGE/ventoy/" \;

copy_boot() {
    name=$1
    if [ -f "$OUT/images/$name" ]; then
        cp -a "$OUT/images/$name" "$STAGE/EFI/BOOT/$name"
    fi
}

copy_boot BOOTAA64.EFI
copy_boot BOOTLOONGARCH64.EFI
copy_boot BOOTMIPS.EFI
rm -f "$STAGE/EFI/BOOT/BOOTMIPS64EL.EFI" "$STAGE/EFI/BOOT/fbaa64.efi" \
    "$STAGE/EFI/BOOT/grubaa64_real.efi"

echo "pack-disk: staged $(stage_bytes) bytes"

ESP="$WORK/esp.img"
DATA="$WORK/data.img"
rm -f "$ESP" "$DATA" "$IMG"
truncate -s $((ESP_SECTORS * 512)) "$ESP"
truncate -s $((DATA_SECTORS * 512)) "$DATA"
# 64 MiB is FAT32 only with 512-byte clusters. The default 4 KiB clusters
# leave fewer than 65525 clusters, and OVMF rejects the volume.
mkfs.vfat -F 32 -s 1 -n VTOYEFI "$ESP"
mkfs.exfat -n Ventoy "$DATA" >/dev/null

mcopy -i "$ESP" -s "$STAGE/EFI" "$STAGE/grub" "$STAGE/ventoy" ::/

truncate -s $(( (P2 + ESP_SECTORS) * 512 )) "$IMG"
sfdisk "$IMG" <<EOF
label: dos
unit: sectors
start=${P1}, size=${DATA_SECTORS}, type=7
start=${P2}, size=${ESP_SECTORS}, type=ef, bootable
EOF
dd if="$DATA" of="$IMG" bs=512 seek="$P1" conv=notrunc status=none
dd if="$ESP" of="$IMG" bs=512 seek="$P2" conv=notrunc status=none

PC="$OUT/prefix-i386-pc/lib/grub/i386-pc"
if [ -f "$OUT/images/core.img" ] && [ -f "$PC/boot.img" ]; then
    # boot.img already names sector 1, and core.img's block list starts at
    # sector 2. grub-bios-setup refuses a regular file and probes the host disk.
    python3 - "$IMG" "$PC/boot.img" "$OUT/images/core.img" <<'PY'
import sys
img, boot, core = sys.argv[1:]
disk = bytearray(open(img, "rb").read())
b = open(boot, "rb").read()
c = open(core, "rb").read()
if len(c) > 2047 * 512:
    raise SystemExit("core.img does not fit in the 1 MiB embedding gap")
disk[0:446] = b[0:446]
disk[512:512 + len(c)] = c
open(img, "wb").write(disk)
print("pack-disk: BIOS boot sector installed")
PY
else
    echo "pack-disk: i386-pc is not built, BIOS boot sector left empty"
fi

echo "pack-disk: $IMG"
ls -lh "$IMG"
