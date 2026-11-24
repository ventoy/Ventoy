#!/bin/sh
# Build GRUB, EDK2, and the loongarch64 tools in Debian trixie.
# GRUB and EDK2 are one source tree for every architecture. CentOS 7 has
# no loongarch64 compiler, so the LoongArch initramfs tools and EfiFs
# drivers are built here too. The other installers and the release
# package are build/ci-centos.sh, which reads build/out from this script.
#
#   docker compose run -T --rm --build trixie

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

if [ "$(id -u)" -ne 0 ]; then
    echo "build/ci-trixie.sh needs root. Run it in privileged docker." >&2
    exit 1
fi

# hexdump counts the shim hash placeholder in build-edk2.sh. On trixie it
# comes from bsdextrautils, which the old single-container script installed.
if ! command -v git >/dev/null 2>&1 || ! command -v patch >/dev/null 2>&1 || ! command -v hexdump >/dev/null 2>&1; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends git patch bsdextrautils
fi

echo "=== GRUB ==="
sh "$ROOT/build/build-grub.sh" \
    x86_64-efi i386-efi arm64-efi loongarch64-efi mips64el-efi i386-pc
test -f "$ROOT/build/out/images/core.img"
test -f "$ROOT/build/out/images/grubx64_real.efi"
test -f "$ROOT/INSTALL/EFI/BOOT/BOOTX64.EFI"
test -f "$ROOT/INSTALL/EFI/BOOT/mmx64.efi"
if ! grep -a -q 'sbat,' "$ROOT/INSTALL/EFI/BOOT/BOOTX64.EFI"; then
    echo "BOOTX64.EFI is not the signed shim" >&2
    exit 1
fi
if grep -a -q 'GNU GRUB' "$ROOT/INSTALL/EFI/BOOT/BOOTX64.EFI"; then
    echo "BOOTX64.EFI must stay the Rocky shim" >&2
    exit 1
fi
test -f "$ROOT/INSTALL/EFI/BOOT/BOOTIA32.EFI"
test -f "$ROOT/INSTALL/EFI/BOOT/grubia32.efi"
test -f "$ROOT/INSTALL/EFI/BOOT/mmia32.efi"
if ! grep -a -q 'sbat,' "$ROOT/INSTALL/EFI/BOOT/BOOTIA32.EFI"; then
    echo "BOOTIA32.EFI is not the signed shim" >&2
    exit 1
fi
if grep -a -q 'GNU GRUB' "$ROOT/INSTALL/EFI/BOOT/BOOTIA32.EFI"; then
    echo "BOOTIA32.EFI must stay the Super-UEFIinSecureBoot-Disk shim" >&2
    exit 1
fi
test -f "$ROOT/build/out/images/grubia32_real.efi"
test -f "$ROOT/build/out/images/BOOTAA64.EFI"
test -f "$ROOT/build/out/images/BOOTLOONGARCH64.EFI"
test -f "$ROOT/build/out/images/BOOTMIPS.EFI"
test -f "$ROOT/build/out/prefix-i386-pc/sbin/grub-bios-setup"
test -f "$ROOT/INSTALL/EFI/BOOT/grubx64_real.efi"
test -f "$ROOT/INSTALL/EFI/BOOT/grubia32_real.efi"
test -f "$ROOT/LiveCDGUI/GRUB/bootx64.efi"
test -f "$ROOT/LiveCDGUI/GRUB/cdrom.img"

echo "=== EDK2 ==="
sh "$ROOT/build/build-edk2.sh" X64 IA32 AARCH64 LOONGARCH64
test -f "$ROOT/INSTALL/EFI/BOOT/fbx64.efi"
test -f "$ROOT/INSTALL/EFI/BOOT/fbia32.efi"
test -f "$ROOT/INSTALL/EFI/BOOT/fbaa64.efi"
test -f "$ROOT/build/out/edk2/X64/Ventoy.efi"
test -f "$ROOT/build/out/edk2/IA32/Ventoy.efi"
test -f "$ROOT/build/out/edk2/AARCH64/Ventoy.efi"
test -f "$ROOT/build/out/edk2/LOONGARCH64/Ventoy.efi"

echo "=== EfiFs drivers ==="
# x86 and arm64 keep the iso9660/udf drivers already in INSTALL/ventoy.
# Only LoongArch has no upstream driver.
sh "$ROOT/build/build-efifs.sh" LOONGARCH64
test -f "$ROOT/build/out/efifs/iso9660_loongarch64.efi"
test -f "$ROOT/build/out/efifs/udf_loongarch64.efi"

echo "=== loongarch64 initramfs tools ==="
# x86, arm64 and mips64 keep the binaries already in IMG/cpio_*.
# A previous local build may have left glibc tools in build/out; those
# would override the upstream blobs inside mkcpio.sh, so drop them.
# Only LoongArch has no upstream initramfs.
for arch in x86_64 i386 aarch64 mips64el; do
    rm -rf "$ROOT/build/out/tools/$arch" "$ROOT/build/out/initramfs/$arch"
done
if [ ! -x "$ROOT/build/out/tools/loongarch64/vtoytool" ] || \
   [ ! -f "$ROOT/build/out/initramfs/loongarch64/busybox.xz" ]; then
    sh "$ROOT/build/build-user.sh" loongarch64
    sh "$ROOT/build/build-init-busybox.sh" loongarch64
fi
for bin in dmsetup unsquashfs vtoy_fuse_iso vtoytool vblade lunzip lz4cat zstdcat xzminidec; do
    test -f "$ROOT/build/out/tools/loongarch64/$bin"
done
test -f "$ROOT/build/out/initramfs/loongarch64/busybox.xz"
test -f "$ROOT/build/out/initramfs/loongarch64/vtchmod"
test -f "$ROOT/build/out/tools/loongarch64/ash"

echo "=== vtchmod for the packed initramfs ==="
# sbin/init decompresses busybox and runs vtchmod before busybox --install.
# Mainline compiles these in BUSYBOX/chmod/build.sh. The blobs in git do not
# include them, and this image does not run that script. Without vtchmod the
# decompressed busybox stays mode 644, so the hook cannot install grep, xz,
# or ln. Do not build busybox.xz here: that would replace the upstream blob.
compile_vtchmod() {
    arch=$1
    cross=$2
    out="$ROOT/build/out/initramfs/$arch"
    mkdir -p "$out"
    "${cross}gcc" -static -Os "$ROOT/BUSYBOX/chmod/vtchmod.c" -o "$out/vtchmod"
    "${cross}strip" --strip-all "$out/vtchmod" || true
    chmod 755 "$out/vtchmod"
    test -x "$out/vtchmod"
}
compile_vtchmod x86_64 ""
compile_vtchmod i386 i686-linux-gnu-
compile_vtchmod aarch64 aarch64-linux-gnu-
compile_vtchmod mips64el mips64el-linux-gnuabi64-

echo "=== vtoytool for the packed initramfs ==="
# vtoydump accepts a 4 MiB or 64 MiB VTOYEFI partition. A 32 MiB ESP is
# rejected. mkcpio prefers this binary over the blob in VtoyTool/vtoytool.
# Do not build the other tools here.
compile_vtoytool() {
    arch=$1
    cross=$2
    extra=$3
    out="$ROOT/build/out/tools/$arch"
    if [ -n "$cross" ]; then
        cc="${cross}gcc"
        strip="${cross}strip"
    else
        cc=/usr/bin/gcc
        strip=/usr/bin/strip
    fi
    mkdir -p "$out"
    echo "compile vtoytool $arch with $cc $extra"
    # shellcheck disable=SC2086
    "$cc" -Os -static -D_FILE_OFFSET_BITS=64 -DBUILD_VTOY_TOOL \
        $extra -I"$ROOT/VtoyTool/BabyISO" \
        "$ROOT"/VtoyTool/*.c "$ROOT"/VtoyTool/BabyISO/*.c \
        -o "$out/vtoytool"
    "$strip" --strip-all "$out/vtoytool" || true
    chmod 755 "$out/vtoytool"
    test -x "$out/vtoytool"
}
compile_vtoytool x86_64 "" ""
compile_vtoytool i386 i686-linux-gnu- ""
compile_vtoytool aarch64 aarch64-linux-gnu- ""
compile_vtoytool mips64el mips64el-linux-gnuabi64- "-mabi=64"

echo "=== loongarch64 host installer ==="
# A LoongArch machine running Ventoy2Disk.sh needs its own ash, xzcat,
# vtoycli and V2DServer. CentOS 7 cannot compile them.
sh "$ROOT/build/build-host-loongarch.sh"

echo "trixie outputs are in build/out. Pack with build/ci-centos.sh."
