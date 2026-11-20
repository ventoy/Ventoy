#!/bin/sh
# Build the non-LoongArch installers and pack the release on CentOS 7.
# GRUB, EDK2, and the loongarch64 tools must already be in build/out.
# This script does not build GRUB 2.04 or EDK2 201911.
#
#   docker compose run -T --rm --build centos

set -u

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

if [ "$(id -u)" -ne 0 ]; then
    echo "build/ci-centos.sh needs root. Run it in privileged docker." >&2
    exit 1
fi

LOOP=
mnt=
cleanup() {
    if [ -n "${mnt:-}" ] && mountpoint -q "$mnt" 2>/dev/null; then
        umount "$mnt" || umount -l "$mnt" || true
    fi
    if [ -n "${LOOP:-}" ]; then
        partx -d "$LOOP" 2>/dev/null || true
        losetup -d "$LOOP" || true
    fi
}
trap cleanup EXIT
set -e

if command -v python3 >/dev/null 2>&1; then
    PY=python3
else
    PY=python
fi

need_built() {
    if [ ! -f "$1" ]; then
        echo "missing $1" >&2
        echo "build it first: docker compose run -T --rm --build trixie" >&2
        exit 1
    fi
}
need_built "$ROOT/build/out/images/core.img"
need_built "$ROOT/build/out/images/grubx64_real.efi"
need_built "$ROOT/build/out/images/grubia32_real.efi"
need_built "$ROOT/build/out/images/BOOTAA64.EFI"
need_built "$ROOT/build/out/images/BOOTLOONGARCH64.EFI"
need_built "$ROOT/build/out/images/BOOTMIPS.EFI"
need_built "$ROOT/build/out/edk2/X64/Ventoy.efi"
need_built "$ROOT/build/out/edk2/IA32/Ventoy.efi"
need_built "$ROOT/build/out/edk2/AARCH64/Ventoy.efi"
need_built "$ROOT/build/out/edk2/LOONGARCH64/Ventoy.efi"
need_built "$ROOT/build/out/edk2/LOONGARCH64/VtoyUtil.efi"
need_built "$ROOT/build/out/efifs/iso9660_loongarch64.efi"
need_built "$ROOT/build/out/efifs/udf_loongarch64.efi"
need_built "$ROOT/build/out/initramfs/loongarch64/busybox.xz"
need_built "$ROOT/build/out/tools/loongarch64/ash"
need_built "$ROOT/INSTALL/tool/loongarch64/V2DServer"
need_built "$ROOT/INSTALL/tool/loongarch64/ash"
need_built "$ROOT/INSTALL/tool/loongarch64/xzcat"
need_built "$ROOT/INSTALL/tool/loongarch64/vtoycli"
need_built "$ROOT/INSTALL/tool/loongarch64/mkexfatfs"
need_built "$ROOT/INSTALL/tool/loongarch64/Ventoy2Disk.gtk3"
need_built "$ROOT/INSTALL/VentoyGUI.loongarch64"

echo "=== CentOS packages ==="
# p7zip is in the EPEL 7 archive. The EL7 key is expired, so the repo
# does not check signatures. It is only used to unpack 7-Zip 19.00.
cat > /etc/yum.repos.d/epel7-archive.repo <<'EOF'
[epel7-archive]
name=EPEL 7 archive
baseurl=https://archives.fedoraproject.org/pub/archive/epel/7/$basearch
enabled=1
gpgcheck=0
EOF
yum -y -q install \
    dosfstools parted xz cpio p7zip file \
    gtk3-devel pkgconfig \
    glibc-static glibc-static.i686

echo "=== GCC 7 toolchains ==="
# Same compilers INSTALL/docker_ci_build.sh downloads for the Linux GUI.
# The old GRUB and EDK2 tarballs are not used.
tool_url=https://github.com/ventoy/vtoytoolchain/releases/download/1.0
fetch_opt() {
    name=$1
    if [ ! -s "/opt/$name" ]; then
        wget -O "/opt/$name.part" "$tool_url/$name"
        mv "/opt/$name.part" "/opt/$name"
    fi
}
if [ ! -d /opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu ]; then
    fetch_opt gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz
    tar -xf /opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu.tar.xz -C /opt
fi
if [ ! -d /opt/mips-loongson-gcc7.3-linux-gnu ]; then
    fetch_opt mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz
    tar -xf /opt/mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz -C /opt
fi
export PATH="$PATH:/opt/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin:/opt/mips-loongson-gcc7.3-linux-gnu/2019.06-29/bin"
command -v aarch64-linux-gnu-gcc >/dev/null
command -v mips-linux-gnu-gcc >/dev/null
echo 'int main(void){return 0;}' | gcc -m32 -x c -o /tmp/vt-m32 -
rm -f /tmp/vt-m32

echo "=== Linux web installers ==="
(
    cd "$ROOT/LinuxGUI"
    sh language.sh
    # bash -e stops when a compiler fails. The script itself does not.
    bash -e ./build.sh
)
check_elf() {
    bin=$1
    needle=$2
    if [ ! -f "$bin" ]; then
        echo "missing $bin" >&2
        exit 1
    fi
    # Debian file(1) says "Intel i386". CentOS 7 file(1) says "Intel 80386".
    if ! file "$bin" | grep -E -q "$needle"; then
        echo "$bin is not $needle" >&2
        file "$bin" >&2
        exit 1
    fi
    echo "ok $bin"
}
check_elf "$ROOT/INSTALL/tool/x86_64/V2DServer" "x86-64"
check_elf "$ROOT/INSTALL/tool/i386/V2DServer" "i386|80386"
check_elf "$ROOT/INSTALL/tool/aarch64/V2DServer" "aarch64"
check_elf "$ROOT/INSTALL/tool/mips64el/V2DServer" "MIPS"
check_loongarch_elf() {
    bin=$1
    if ! "$PY" -c 'import struct,sys
d=open(sys.argv[1],"rb").read(20)
m=struct.unpack_from("<H", d, 18)[0]
sys.exit(0 if m == 258 else 1)' "$bin"; then
        echo "$bin is not a LoongArch ELF" >&2
        exit 1
    fi
    echo "ok $bin"
}
check_loongarch_elf "$ROOT/INSTALL/tool/loongarch64/V2DServer"

echo "=== Plugson ==="
# The recognition of a 64 MiB ESP is compiled into Plugson.
( cd "$ROOT/Plugson" && bash -e ./build.sh )
check_elf "$ROOT/INSTALL/tool/x86_64/Plugson" "x86-64"
check_elf "$ROOT/INSTALL/tool/i386/Plugson" "i386|80386"
check_elf "$ROOT/INSTALL/tool/aarch64/Plugson" "aarch64"
check_elf "$ROOT/INSTALL/tool/mips64el/Plugson" "MIPS"
check_loongarch_elf "$ROOT/INSTALL/tool/loongarch64/Plugson"

echo "=== GTK3 x86_64 ==="
# Upstream ventoy_pack.sh ships the prebuilt GTK binaries. The 64 MiB ESP
# check is compiled into this one, so CI rebuilds the host GTK3 only.
( cd "$ROOT/LinuxGUI" && bash ./build_gtk.sh x86_64-gtk3 )
test -f "$ROOT/INSTALL/VentoyGUI.x86_64"
test -f "$ROOT/INSTALL/tool/x86_64/Ventoy2Disk.gtk3"
check_elf "$ROOT/INSTALL/VentoyGUI.x86_64" "x86-64"
check_loongarch_elf "$ROOT/INSTALL/VentoyGUI.loongarch64"
check_loongarch_elf "$ROOT/INSTALL/tool/loongarch64/Ventoy2Disk.gtk3"

echo "=== VentoyVlnk.sh ==="
( cd "$ROOT/Vlnk" && sh pack.sh )
test -f "$ROOT/INSTALL/VentoyVlnk.sh"
test -f "$ROOT/INSTALL/VentoyVlnk.exe"

echo "=== host tools ==="
# vtoycli stays the binary already in INSTALL/tool. It does not hard-code
# the ESP length. vtoygpt is not committed in this tree; compile it only
# when the prebuilt file is absent.
test -s "$ROOT/INSTALL/tool/x86_64/vtoycli"
if [ ! -s "$ROOT/INSTALL/tool/x86_64/vtoygpt" ]; then
    (
        cd "$ROOT/vtoygpt"
        build_vtoygpt() {
            dest=$1
            shift
            # shellcheck disable=SC2086
            "$@" -Os -static -D_FILE_OFFSET_BITS=64 \
                vtoygpt.c crc32.c -o "$dest"
            test -s "$dest"
        }
        build_vtoygpt "$ROOT/INSTALL/tool/x86_64/vtoygpt" gcc
        strip --strip-all "$ROOT/INSTALL/tool/x86_64/vtoygpt"
        build_vtoygpt "$ROOT/INSTALL/tool/i386/vtoygpt" gcc -m32
        strip --strip-all "$ROOT/INSTALL/tool/i386/vtoygpt"
        build_vtoygpt "$ROOT/INSTALL/tool/aarch64/vtoygpt" aarch64-linux-gnu-gcc
        aarch64-linux-gnu-strip --strip-all "$ROOT/INSTALL/tool/aarch64/vtoygpt"
        build_vtoygpt "$ROOT/INSTALL/tool/mips64el/vtoygpt" \
            mips-linux-gnu-gcc -mips64r2 -mabi=64
        mips-linux-gnu-strip --strip-all "$ROOT/INSTALL/tool/mips64el/vtoygpt"
    )
fi

echo "=== plugson web UI ==="
# Plugson/pack.sh also demands the Windows exes. The tar itself is www/.
plugson_stage=$ROOT/build/work/plugson
rm -rf "$plugson_stage"
mkdir -p "$plugson_stage"
cp -a "$ROOT/Plugson/www" "$plugson_stage/www"
: > "$plugson_stage/www/helplist"
ls -1 "$ROOT/INSTALL/grub/help" | while read -r line; do
    printf '%s' "$(printf '%s' "$line" | cut -c1-5)" >> "$plugson_stage/www/helplist"
done
: > "$plugson_stage/www/menulist"
ls -1 "$ROOT/INSTALL/grub/menu" | while read -r line; do
    printf '%s' "$(printf '%s' "$line" | cut -c1-5)" >> "$plugson_stage/www/menulist"
done
date '+%Y%m%d %H:%M:%S' > "$plugson_stage/www/buildtime"
tar -C "$plugson_stage" -cf "$plugson_stage/www.tar" www
xz --check=crc32 "$plugson_stage/www.tar"
mv "$plugson_stage/www.tar.xz" "$ROOT/INSTALL/tool/plugson.tar.xz"
test -s "$ROOT/INSTALL/tool/plugson.tar.xz"

echo "=== initramfs ==="
(
    cd "$ROOT/IMG"
    VTOY_CPIO_DEST="$ROOT/INSTALL/ventoy" bash ./mkcpio.sh
)
for cpio in ventoy.cpio ventoy_x86.cpio ventoy_arm64.cpio ventoy_mips64.cpio ventoy_loongarch64.cpio; do
    test -f "$ROOT/INSTALL/ventoy/$cpio"
done
# The hook executes these before anything can chmod them.
for need in \
    ventoy_x86.cpio:vtchmod64 \
    ventoy_x86.cpio:vtchmod32 \
    ventoy_arm64.cpio:vtchmodaa64 \
    ventoy_mips64.cpio:vtchmodm64e \
    ventoy_loongarch64.cpio:vtchmodla64
do
    cpio=${need%%:*}
    name=${need#*:}
    if ! cpio -t < "$ROOT/INSTALL/ventoy/$cpio" | grep -q "$name"; then
        echo "initramfs $cpio is missing $name" >&2
        exit 1
    fi
done

echo "=== 7za ==="
# Official 1.1.18 ships 7-Zip 19.00 extra. ventoy-7zip-2605 is a later build
# and its hashes do not match that release.
# CentOS 7 p7zip provides 7za. Debian's 7zz is not in this image.
z7=$("$ROOT/build/download.sh" "https://www.7-zip.org/a/7z1900-extra.7z" "7z1900-extra.7z")
zdir=$ROOT/build/work/7zextra
rm -rf "$zdir"
mkdir -p "$zdir"
if command -v 7z >/dev/null 2>&1; then
    7z x -y "-o$zdir" "$z7" 7za.exe x64/7za.exe >/dev/null
elif command -v 7zz >/dev/null 2>&1; then
    7zz x -y "-o$zdir" "$z7" 7za.exe x64/7za.exe >/dev/null
elif command -v 7za >/dev/null 2>&1; then
    7za x -y "-o$zdir" "$z7" 7za.exe x64/7za.exe >/dev/null
else
    echo "cannot unpack 7z1900-extra.7z" >&2
    exit 1
fi
hash32=$(sha256sum "$zdir/7za.exe" | awk '{print $1}')
hash64=$(sha256sum "$zdir/x64/7za.exe" | awk '{print $1}')
if [ "$hash32" != "ea308c76a2f927b160a143d94072b0dce232e04b751f0c6432a94e05164e716d" ]; then
    echo "7za.exe hash is $hash32" >&2
    exit 1
fi
if [ "$hash64" != "8117e40ee7f824f63373a4f5625bb62749f69159d0c449b3ce2f35aad3b83549" ]; then
    echo "x64/7za.exe hash is $hash64" >&2
    exit 1
fi
mkdir -p "$ROOT/INSTALL/ventoy/7z/32" "$ROOT/INSTALL/ventoy/7z/64"
rm -f "$ROOT/INSTALL/ventoy/7z/32/7za" "$ROOT/INSTALL/ventoy/7z/32/7za.xz"
rm -f "$ROOT/INSTALL/ventoy/7z/64/7za" "$ROOT/INSTALL/ventoy/7z/64/7za.xz"
cp -a "$zdir/7za.exe" "$ROOT/INSTALL/ventoy/7z/32/7za"
cp -a "$zdir/x64/7za.exe" "$ROOT/INSTALL/ventoy/7z/64/7za"
xz --check=crc32 "$ROOT/INSTALL/ventoy/7z/32/7za"
xz --check=crc32 "$ROOT/INSTALL/ventoy/7z/64/7za"
test -s "$ROOT/INSTALL/ventoy/7z/32/7za.xz"
test -s "$ROOT/INSTALL/ventoy/7z/64/7za.xz"

echo "=== vtloopex ==="
(
    cd "$ROOT/IMG"
    bash ./mkloopex.sh
)
test -s "$ROOT/INSTALL/ventoy/vtloopex.cpio"

echo "=== installer disk image ==="
ver=$(sed -n 's/.*VENTOY_VERSION="\([^"]*\)".*/\1/p' "$ROOT/INSTALL/grub/grub.cfg" | head -1)
if [ -z "$ver" ]; then
    echo "VENTOY_VERSION is missing from INSTALL/grub/grub.cfg" >&2
    exit 1
fi

stage=$ROOT/build/out/ci-pack/stage
rm -rf "$ROOT/build/out/ci-pack"
# The installed ESP is 64 MiB FAT32. Modules follow install.sh: the image
# embeds the menu set, and the ESP gets the rest. arm/mips/loong files are xz.
mkdir -p "$stage/bios/i386-pc" "$stage/grub"
cp -a "$ROOT/INSTALL/grub/." "$stage/grub/"
# shellcheck disable=SC1091
. "$ROOT/build/grub-module-sets.sh"
for plat in i386-pc x86_64-efi i386-efi arm64-efi loongarch64-efi mips64el-efi; do
    grub_install_esp_mods "$plat" \
        "$ROOT/build/out/prefix-$plat/lib/grub/$plat" \
        "$stage/grub/$plat"
done
pc=$ROOT/build/out/prefix-i386-pc/lib/grub/i386-pc
cp -a "$pc"/*.mod "$pc"/*.lst "$pc"/*.img "$stage/bios/i386-pc/"
cp -a "$ROOT/build/out/images/core.img" "$stage/bios/i386-pc/core.img"
test -f "$stage/bios/i386-pc/boot.img"
test -f "$stage/bios/i386-pc/core.img"

img=$ROOT/build/out/ci-pack/img.bin
dd if=/dev/zero of="$img" bs=1M count=256
# CentOS 7 losetup has no -P. Attach the image, then partx after fdisk.
err=$(mktemp)
if LOOP=$(losetup -f --show -P "$img" 2>"$err"); then
    rm -f "$err"
else
    if ! grep -E -q 'invalid option|unrecognized option|unknown option' "$err"; then
        cat "$err" >&2
        rm -f "$err"
        exit 1
    fi
    rm -f "$err"
    LOOP=$(losetup -f --show "$img")
fi
i=0
while ! grep -q 524288 "/sys/block/${LOOP#/dev/}/size" 2>/dev/null; do
    i=$((i + 1))
    if [ "$i" -gt 30 ]; then
        echo "loop device $LOOP did not appear" >&2
        exit 1
    fi
    sleep 1
done

# ventoy_lib.sh uses expr, which exits 1 when the remainder is 0.
cd "$ROOT/build/out/ci-pack"
# shellcheck disable=SC1091
. "$ROOT/INSTALL/tool/ventoy_lib.sh"
set +e
format_ventoy_disk_mbr 0 "$LOOP" fdisk
fmt_status=$?
set -e
if [ "$fmt_status" -ne 0 ]; then
    echo "format_ventoy_disk_mbr failed" >&2
    exit 1
fi
if [ -z "${part2_start_sector:-}" ]; then
    echo "part2_start_sector was not set" >&2
    exit 1
fi
i=0
while [ ! -e "/sys/class/block/${LOOP#/dev/}p2/size" ]; do
    partx -u "$LOOP" 2>/dev/null || partx -a "$LOOP" 2>/dev/null || true
    i=$((i + 1))
    if [ "$i" -gt 30 ]; then
        echo "partition ${LOOP}p2 did not appear" >&2
        exit 1
    fi
    sleep 1
done
part2_sectors=$(cat "/sys/class/block/${LOOP#/dev/}p2/size")
if [ "$part2_sectors" != "131072" ]; then
    echo "ESP is $part2_sectors sectors, expected 131072 (64 MiB)" >&2
    exit 1
fi

# grub-bios-setup resolves the host directory to a block device. In Docker
# that directory is overlay or tmpfs, and the call exits 1 with
# "failed to get canonical path". grub-mkimage already points boot.img at
# sector 1 and core.img's block list at sector 2, which is the embedding gap
# before partition 1. Copy those bytes in and keep the partition table.
# CentOS 7 has Python 2.7, which has no os.pread or os.pwrite.
"$PY" - "$LOOP" "$stage/bios/i386-pc/boot.img" "$stage/bios/i386-pc/core.img" <<'PY'
import os, sys
dev, boot, core = sys.argv[1:]
b = bytearray(open(boot, "rb").read())
c = open(core, "rb").read()
if len(b) != 512:
    raise SystemExit("boot.img is not 512 bytes")
if len(c) > 2047 * 512:
    raise SystemExit("core.img does not fit in the embedding gap")

def pread(fd, n, off):
    os.lseek(fd, off, os.SEEK_SET)
    buf = b""
    while len(buf) < n:
        chunk = os.read(fd, n - len(buf))
        if not chunk:
            break
        buf += chunk
    return buf

def pwrite(fd, data, off):
    os.lseek(fd, off, os.SEEK_SET)
    view = data
    while view:
        wrote = os.write(fd, view)
        if wrote <= 0:
            raise SystemExit("short write")
        view = view[wrote:]

fd = os.open(dev, os.O_RDWR)
old = bytearray(pread(fd, 512, 0))
# Same hard-disk drive-check NOP and BPB/partition preservation as
# grub-bios-setup. Bytes 0x1fe-0x1ff stay the boot.img signature.
b[0x03:0x5a] = old[0x03:0x5a]
b[0x66] = 0x90
b[0x67] = 0x90
b[0x1b8:0x1fe] = old[0x1b8:0x1fe]
pwrite(fd, bytes(b) if not isinstance(b, str) else b, 0)
pwrite(fd, c, 512)
os.fsync(fd)
os.close(fd)
print("BIOS boot sector installed")
PY

mnt=$ROOT/build/out/ci-pack/mnt
rm -rf "$mnt"
mkdir -p "$mnt"
mount "${LOOP}p2" "$mnt"

mkdir -p "$mnt/grub"
# vfat has no Unix owners. cp -a then exits 1 even though the bytes were written.
cp -a --no-preserve=ownership "$stage/grub/." "$mnt/grub/"
test -f "$mnt/grub/grub.cfg"
for plat in i386-pc x86_64-efi i386-efi arm64-efi loongarch64-efi mips64el-efi; do
    test -f "$mnt/grub/$plat/normal.mod"
done

(
    cd "$mnt/grub"
    tar czf help.tar.gz ./help/
    rm -rf ./help
    vtlangtitle=$(grep VTLANG_LANGUAGE_NAME menu/zh_CN.json | awk -F\" '{print $4}')
    echo "menuentry \"zh_CN  -  $vtlangtitle\" --class=menu_lang_item --class=debug_menu_lang --class=F5tool {" >> menulang.cfg
    echo "    vt_load_menu_lang zh_CN" >> menulang.cfg
    echo "}" >> menulang.cfg
    ls -1 menu/ | grep -v 'zh_CN' | sort | while read -r vtlang; do
        vtlangname=${vtlang%.*}
        vtlangtitle=$(grep VTLANG_LANGUAGE_NAME "menu/$vtlang" | awk -F\" '{print $4}')
        echo "menuentry \"$vtlangname  -  $vtlangtitle\" --class=menu_lang_item --class=debug_menu_lang --class=F5tool {" >> menulang.cfg
        echo "    vt_load_menu_lang $vtlangname" >> menulang.cfg
        echo "}" >> menulang.cfg
    done
    echo "menuentry \"\$VTLANG_RETURN_PREVIOUS\" --class=vtoyret VTOY_RET {" >> menulang.cfg
    echo "        echo \"Return ...\"" >> menulang.cfg
    echo "}" >> menulang.cfg
    tar czf menu.tar.gz ./menu/
    rm -rf ./menu
)
test -f "$mnt/grub/help.tar.gz"
test -f "$mnt/grub/menu.tar.gz"
rm -f "$mnt/grub/i386-pc/"*.img

cp -a --no-preserve=ownership "$ROOT/INSTALL/ventoy" "$mnt/"
cp -a --no-preserve=ownership "$ROOT/INSTALL/EFI" "$mnt/"
# INSTALL/EFI and INSTALL/ventoy still contain previously built files.
# Replace every boot file this run compiled.
install_built() {
    src=$1
    dest=$2
    if [ ! -f "$src" ]; then
        echo "missing built file $src" >&2
        exit 1
    fi
    cp -a --no-preserve=ownership "$src" "$dest"
}
imgdir=$ROOT/build/out/images
# BOOTX64.EFI is the Rocky shim copied with INSTALL/EFI. Do not replace it
# with GRUB. grubx64_real.efi is the image fbx64.efi chainloads.
install_built "$imgdir/grubx64_real.efi" "$mnt/EFI/BOOT/grubx64_real.efi"
if ! cmp -s "$ROOT/INSTALL/EFI/BOOT/BOOTX64.EFI" "$mnt/EFI/BOOT/BOOTX64.EFI"; then
    echo "ESP BOOTX64.EFI is not the Rocky shim" >&2
    exit 1
fi
# BOOTIA32.EFI is the Super-UEFIinSecureBoot-Disk shim copied with
# INSTALL/EFI. Do not replace it with GRUB. grubia32.efi chainloads
# grubia32_real.efi.
install_built "$imgdir/grubia32_real.efi" "$mnt/EFI/BOOT/grubia32_real.efi"
if ! cmp -s "$ROOT/INSTALL/EFI/BOOT/BOOTIA32.EFI" "$mnt/EFI/BOOT/BOOTIA32.EFI"; then
    echo "ESP BOOTIA32.EFI is not the upstream shim" >&2
    exit 1
fi
if ! cmp -s "$ROOT/INSTALL/EFI/BOOT/grubia32.efi" "$mnt/EFI/BOOT/grubia32.efi"; then
    echo "ESP grubia32.efi is not the upstream loader" >&2
    exit 1
fi
install_built "$imgdir/BOOTAA64.EFI" "$mnt/EFI/BOOT/BOOTAA64.EFI"
install_built "$imgdir/BOOTLOONGARCH64.EFI" "$mnt/EFI/BOOT/BOOTLOONGARCH64.EFI"
install_built "$imgdir/BOOTMIPS.EFI" "$mnt/EFI/BOOT/BOOTMIPS.EFI"
# Upstream AA64 boots BOOTAA64.EFI directly. IA32 uses grubia32.efi,
# not a second shim named fbia32.efi. BOOTMIPS64EL is not in the release.
rm -f "$mnt/EFI/BOOT/grubaa64_real.efi" "$mnt/EFI/BOOT/fbaa64.efi" \
    "$mnt/EFI/BOOT/fbia32.efi" "$mnt/EFI/BOOT/BOOTMIPS64EL.EFI"
edk=$ROOT/build/out/edk2
# Every architecture's Ventoy.efi has to accept a 64 MiB ESP. The other
# prebuilt EFI payloads stay the files copied from INSTALL/ventoy.
install_built "$edk/X64/Ventoy.efi" "$mnt/ventoy/ventoy_x64.efi"
install_built "$edk/IA32/Ventoy.efi" "$mnt/ventoy/ventoy_ia32.efi"
install_built "$edk/AARCH64/Ventoy.efi" "$mnt/ventoy/ventoy_aa64.efi"
install_built "$edk/LOONGARCH64/Ventoy.efi" "$mnt/ventoy/ventoy_loongarch64.efi"
install_built "$edk/LOONGARCH64/VtoyUtil.efi" "$mnt/ventoy/vtoyutil_loongarch64.efi"
efs=$ROOT/build/out/efifs
install_built "$efs/iso9660_loongarch64.efi" "$mnt/ventoy/iso9660_loongarch64.efi"
install_built "$efs/udf_loongarch64.efi" "$mnt/ventoy/udf_loongarch64.efi"
for cpio in ventoy.cpio ventoy_x86.cpio ventoy_arm64.cpio ventoy_mips64.cpio ventoy_loongarch64.cpio; do
    test -f "$mnt/ventoy/$cpio"
done
test -f "$mnt/EFI/BOOT/grubx64_real.efi"
test -f "$mnt/EFI/BOOT/grubia32_real.efi"
test -f "$mnt/EFI/BOOT/fbx64.efi"
test ! -e "$mnt/EFI/BOOT/fbia32.efi"
test ! -e "$mnt/EFI/BOOT/fbaa64.efi"
test ! -e "$mnt/EFI/BOOT/BOOTMIPS64EL.EFI"

mkdir -p "$mnt/tool"
dd bs=1024 count=16 if="$ROOT/INSTALL/tool/i386/vtoycli" of="$mnt/tool/mount.exfat-fuse_i386"
dd bs=1024 count=16 if="$ROOT/INSTALL/tool/x86_64/vtoycli" of="$mnt/tool/mount.exfat-fuse_x86_64"
dd bs=1024 count=16 if="$ROOT/INSTALL/tool/aarch64/vtoycli" of="$mnt/tool/mount.exfat-fuse_aarch64"
cp -a --no-preserve=ownership "$ROOT/INSTALL/tool/create_ventoy_iso_part_dm.sh" "$mnt/tool/"
if [ -f "$ROOT/INSTALL/tool/ENROLL_THIS_KEY_IN_MOKMANAGER.cer" ]; then
    cp -a --no-preserve=ownership "$ROOT/INSTALL/tool/ENROLL_THIS_KEY_IN_MOKMANAGER.cer" "$mnt/"
fi

# BOOTX64.EFI is already signed by Microsoft and must not be signed again.
# VtoyShim checks the on-disk GRUB hash, so sign grub*_real.efi first.
# Without VENTOY_CERT_KEY and VENTOY_CERT_PEM this is the same NO-CA path
# as INSTALL/ventoy_pack.sh.
sign_efi() {
    efi=$1
    if [ ! -f "$efi" ]; then
        echo "### $efi non-exist"
        return 0
    fi
    if [ -z "${VENTOY_CERT_KEY:-}" ] || [ -z "${VENTOY_CERT_PEM:-}" ]; then
        echo "### $efi NO-CA"
        return 0
    fi
    if ! command -v sbsign >/dev/null 2>&1; then
        echo "sbsign is not installed" >&2
        exit 1
    fi
    rm -f "${efi}.signed"
    sbsign --key "$VENTOY_CERT_KEY" --cert "$VENTOY_CERT_PEM" --output "${efi}.signed" "$efi"
    mv "${efi}.signed" "$efi"
    echo "### $efi signed"
}

# Each shim's placeholder becomes the hash of the GRUB image it loads.
inject_grub_hash() {
    shim=$1
    grub=$2
    grub_sha256=$(sha256sum "$mnt/EFI/BOOT/$grub" | awk '{print $1}')
    magic_cnt=$(hexdump -C "$mnt/EFI/BOOT/$shim" | grep -c '26 26 26 26 26 26 26 26' || true)
    if [ "$magic_cnt" -ne 1 ]; then
        echo "$shim hash placeholder count is $magic_cnt, expected 1" >&2
        exit 1
    fi
    magic_off_hex=$(hexdump -C "$mnt/EFI/BOOT/$shim" | grep '26 26 26 26 26 26 26 26' | awk '{print $1}')
    magic_off=$(printf '%u' "0x${magic_off_hex}")
    echo "$shim <- $grub $grub_sha256"
    hashbin=$ROOT/build/out/ci-pack/grub.sha256
    "$PY" -c 'import binascii,sys; open(sys.argv[1],"wb").write(binascii.unhexlify(sys.argv[2]))' \
        "$hashbin" "$grub_sha256"
    dd if="$hashbin" bs=1 count=32 of="$mnt/EFI/BOOT/$shim" seek="$magic_off" conv=notrunc
}
sign_efi "$mnt/EFI/BOOT/grubx64_real.efi"
sign_efi "$mnt/EFI/BOOT/grubia32_real.efi"
inject_grub_hash fbx64.efi grubx64_real.efi
sign_efi "$mnt/EFI/BOOT/fbx64.efi"

sync
umount "$mnt"
mnt=

pkg=$ROOT/build/out/ci-pack/ventoy-$ver
rm -rf "$pkg"
mkdir -p "$pkg/boot" "$pkg/ventoy"
echo "$ver" > "$pkg/ventoy/version"
dd if="$LOOP" of="$pkg/boot/boot.img" bs=1 count=512
dd if="$LOOP" of="$pkg/boot/core.img" bs=512 count=2047 skip=1
xz --check=crc32 "$pkg/boot/core.img"
dd if="$LOOP" of="$pkg/ventoy/ventoy.disk.img" bs=512 count="$VENTOY_SECTOR_NUM" skip="$part2_start_sector"
img_bytes=$(stat -c '%s' "$pkg/ventoy/ventoy.disk.img")
if [ "$img_bytes" != "67108864" ]; then
    echo "ventoy.disk.img is $img_bytes bytes, expected 67108864 (64 MiB)" >&2
    exit 1
fi
xz --check=crc32 "$pkg/ventoy/ventoy.disk.img"

partx -d "$LOOP" 2>/dev/null || true
losetup -d "$LOOP"
LOOP=

echo "=== linux tarball ==="
cp -a "$ROOT/INSTALL/tool" "$pkg/"
rm -f "$pkg/ENROLL_THIS_KEY_IN_MOKMANAGER.cer"
cp -a "$ROOT/INSTALL/Ventoy2Disk.sh" "$pkg/"
cp -a "$ROOT/INSTALL/VentoyWeb.sh" "$pkg/"
cp -a "$ROOT/INSTALL/VentoyPlugson.sh" "$pkg/"
cp -a "$ROOT/INSTALL/VentoyVlnk.sh" "$pkg/"
cp -a "$ROOT/INSTALL/VentoyGUI."* "$pkg/" 2>/dev/null || true
cp -a "$ROOT/INSTALL/README" "$pkg/"
cp -a "$ROOT/INSTALL/plugin" "$pkg/"
cp -a "$ROOT/INSTALL/CreatePersistentImg.sh" "$pkg/"
cp -a "$ROOT/INSTALL/ExtendPersistentImg.sh" "$pkg/"
dos2unix -q "$pkg/Ventoy2Disk.sh" "$pkg/VentoyWeb.sh" "$pkg/VentoyPlugson.sh" "$pkg/VentoyVlnk.sh"
dos2unix -q "$pkg/CreatePersistentImg.sh" "$pkg/ExtendPersistentImg.sh"
cp -a "$ROOT/LinuxGUI/WebUI" "$pkg/"
sed 's/.*SCRIPT_DEL_THIS \(.*\)/\1/g' -i "$pkg/WebUI/index.html"
cp -a "$ROOT/LANGUAGES/languages.json" "$pkg/tool/"

for d in i386 x86_64 aarch64 mips64el loongarch64; do
    for file in "$pkg/tool/$d"/*; do
        name=$(basename "$file")
        if [ "$name" = "xzcat" ]; then
            continue
        fi
        case "$name" in
            Ventoy2Disk*) chmod +x "$file" ;;
            *) xz --check=crc32 "$file" ;;
        esac
    done
done

find "$pkg" -type d -exec chmod 755 "{}" +
find "$pkg" -type f -exec chmod 644 "{}" +
chmod +x "$pkg/Ventoy2Disk.sh" "$pkg/VentoyWeb.sh" "$pkg/VentoyPlugson.sh" "$pkg/VentoyVlnk.sh"
chmod +x "$pkg/tool/"*.sh
for gui in "$pkg"/VentoyGUI*; do
    if [ -f "$gui" ]; then
        chmod +x "$gui"
    fi
done
for d in i386 x86_64 aarch64 mips64el loongarch64; do
    chmod +x "$pkg/tool/$d/xzcat"
    for exe in "$pkg/tool/$d"/Ventoy2Disk.*; do
        if [ -f "$exe" ]; then
            chmod +x "$exe"
        fi
    done
done

rm -f "$ROOT/INSTALL/ventoy-${ver}-linux.tar.gz"
tar -czf "$ROOT/INSTALL/ventoy-${ver}-linux.tar.gz" -C "$ROOT/build/out/ci-pack" "ventoy-${ver}"
test -f "$ROOT/INSTALL/ventoy-${ver}-linux.tar.gz"

echo "=== LiveCD ISO ==="
(
    cd "$ROOT/LiveCDGUI"
    bash ./livecd.sh CI
)
test -f "$ROOT/LiveCDGUI/ventoy-${ver}-livecd.iso"
iso_bytes=$(stat -c '%s' "$ROOT/LiveCDGUI/ventoy-${ver}-livecd.iso")
if [ "$iso_bytes" -lt 1000000 ]; then
    echo "LiveCD ISO is only $iso_bytes bytes" >&2
    exit 1
fi

if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
    chown "$HOST_UID:$HOST_GID" \
        "$ROOT/INSTALL/ventoy-${ver}-linux.tar.gz" \
        "$ROOT/LiveCDGUI/ventoy-${ver}-livecd.iso" || true
    chown -R "$HOST_UID:$HOST_GID" \
        "$ROOT/INSTALL/EFI" \
        "$ROOT/INSTALL/tool" \
        "$ROOT/LiveCD/GRUB" \
        "$ROOT/LiveCDGUI/GRUB" \
        "$ROOT/LiveCDGUI/EXT" \
        "$ROOT/LinuxGUI/WebUI" || true
fi

echo "linux installer: INSTALL/ventoy-${ver}-linux.tar.gz"
echo "livecd iso:      LiveCDGUI/ventoy-${ver}-livecd.iso"
ls -lh "$ROOT/INSTALL/ventoy-${ver}-linux.tar.gz" "$ROOT/LiveCDGUI/ventoy-${ver}-livecd.iso"
