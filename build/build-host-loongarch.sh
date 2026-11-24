#!/bin/sh
# Static installer tools for a LoongArch machine creating a Ventoy disk.
# CentOS 7 has no loongarch64 compiler, so Debian trixie builds these and
# ci-centos.sh only packs INSTALL/tool/loongarch64 into the release.
# The GTK3 UI is linked against libgtk-3-dev:loong64 from
# https://loong13.debian.net/. Qt stays out: its code generator has to run
# on the build machine, and the loong64 qmake cannot.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/build/versions.env"

CC=loongarch64-linux-gnu-gcc
AR=loongarch64-linux-gnu-ar
STRIP=loongarch64-linux-gnu-strip
OUT="$ROOT/INSTALL/tool/loongarch64"
CACHE="$ROOT/.cache/upstream"
WORK="$ROOT/build/work/host-loongarch"

command -v "$CC" >/dev/null
mkdir -p "$OUT" "$CACHE" "$WORK"

is_loongarch() {
    python3 -c 'import struct,sys
d=open(sys.argv[1],"rb").read(20)
sys.exit(0 if struct.unpack_from("<H", d, 18)[0] == 258 else 1)' "$1"
}

echo "=== busybox ash, hexdump, xzcat ==="
src="$CACHE/busybox-${BUSYBOX_VERSION}"
if [ ! -d "$src" ]; then
    if [ ! -f "$CACHE/busybox-${BUSYBOX_VERSION}.tar.bz2" ]; then
        wget -O "$CACHE/busybox-${BUSYBOX_VERSION}.tar.bz2.part" "$BUSYBOX_URL"
        mv "$CACHE/busybox-${BUSYBOX_VERSION}.tar.bz2.part" \
            "$CACHE/busybox-${BUSYBOX_VERSION}.tar.bz2"
    fi
    tar -xf "$CACHE/busybox-${BUSYBOX_VERSION}.tar.bz2" -C "$CACHE"
fi
bb="$WORK/busybox"
rm -rf "$bb"
mkdir -p "$bb"
make -C "$src" O="$bb" defconfig
sed -i \
    -e 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' \
    -e 's/^# CONFIG_XZ is not set/CONFIG_XZ=y/' \
    -e 's/^# CONFIG_XZCAT is not set/CONFIG_XZCAT=y/' \
    -e 's/^# CONFIG_HEXDUMP is not set/CONFIG_HEXDUMP=y/' \
    -e 's/^CONFIG_TC=y/# CONFIG_TC is not set/' \
    -e 's/^CONFIG_FEATURE_TC_.*=y/# &/' \
    "$bb/.config"
if ! grep -q '^CONFIG_STATIC=y$' "$bb/.config"; then
    echo 'CONFIG_STATIC=y' >> "$bb/.config"
fi
yes '' | make -C "$src" O="$bb" oldconfig >/dev/null
make -C "$src" O="$bb" -j"$(nproc)" ARCH=loongarch64 CROSS_COMPILE=loongarch64-linux-gnu-
for app in ash hexdump xzcat; do
    cp -a "$bb/busybox" "$OUT/$app"
    chmod 755 "$OUT/$app"
done

echo "=== vtoycli ==="
fat="$WORK/fat"
rm -rf "$fat"
mkdir -p "$fat"
cp "$ROOT/vtoycli/fat_io_lib/release"/fat*.c \
    "$ROOT/vtoycli/fat_io_lib/release"/fat*.h "$fat/"
(
    cd "$fat"
    "$CC" -O2 -D_FILE_OFFSET_BITS=64 -c fat*.c
    "$AR" rc libfat_io.a ./*.o
)
(
    cd "$ROOT/vtoycli"
    # shellcheck disable=SC2086
    "$CC" -Os -static -D_FILE_OFFSET_BITS=64 \
        vtoycli.c vtoyfat.c vtoygpt.c crc32.c partresize.c \
        -Ifat_io_lib/include "$fat/libfat_io.a" -o "$OUT/vtoycli"
)
"$STRIP" --strip-all "$OUT/vtoycli"

echo "=== vtoygpt ==="
(
    cd "$ROOT/vtoygpt"
    "$CC" -Os -static -D_FILE_OFFSET_BITS=64 vtoygpt.c crc32.c -o "$OUT/vtoygpt"
)
"$STRIP" --strip-all "$OUT/vtoygpt"

echo "=== vlnk ==="
(
    cd "$ROOT/Vlnk"
    "$CC" -Os -static -D_FILE_OFFSET_BITS=64 \
        src/crc32.c src/main_linux.c src/vlnk.c -Isrc -o "$OUT/vlnk"
)
"$STRIP" --strip-all "$OUT/vlnk"

echo "=== mkexfatfs ==="
ex="$WORK/exfat"
rm -rf "$ex"
mkdir -p "$ex"
python3 -c 'import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' \
    "$ROOT/LinuxGUI/Ventoy2Disk/Lib/exfat/exfat-1.3.0.zip" "$ex"
src_ex=$(find "$ex" -mindepth 1 -maxdepth 1 -type d | head -n 1)
printf '%s\n' '#define PACKAGE_VERSION "1.3.0"' > "$src_ex/libexfat/config.h"
# Upstream <exfat.h> is the library header. Keep the CLI main in mkfs/.
"$CC" -Os -static -D_FILE_OFFSET_BITS=64 -DVERSION='"1.3.0"' \
    -I"$src_ex/libexfat" -I"$src_ex/mkfs" \
    "$src_ex"/mkfs/*.c "$src_ex"/libexfat/*.c \
    -o "$OUT/mkexfatfs"
"$STRIP" --strip-all "$OUT/mkexfatfs"

echo "=== V2DServer and Plugson ==="
( cd "$ROOT/LinuxGUI" && bash -e ./build.sh loongarch64 )
rm -f "$ROOT/LinuxGUI/V2Dla64" "$ROOT/LinuxGUI/civetweb.o"
( cd "$ROOT/Plugson" && bash -e ./build.sh loongarch64 )
rm -f "$ROOT/Plugson/Plugsonla64" "$ROOT/Plugson/civetweb.o"

for bin in ash hexdump xzcat vtoycli vtoygpt vlnk mkexfatfs V2DServer Plugson; do
    test -s "$OUT/$bin"
    is_loongarch "$OUT/$bin"
    if readelf -l "$OUT/$bin" | grep -q 'Requesting program interpreter'; then
        echo "$bin is not static" >&2
        exit 1
    fi
    echo "ok $bin"
done

echo "=== GTK3 ==="
sh "$ROOT/build/install-loong64-gtk.sh"
( cd "$ROOT/LinuxGUI" && bash -e ./build_gtk.sh loongarch64 )
rm -f "$ROOT/LinuxGUI/Ventoy2Disk.gtk3_la64" "$ROOT/LinuxGUI/VentoyGUI.loongarch64" "$ROOT/LinuxGUI/civetweb.o"
is_loongarch "$OUT/Ventoy2Disk.gtk3"
is_loongarch "$ROOT/INSTALL/VentoyGUI.loongarch64"
echo "ok Ventoy2Disk.gtk3"
echo "ok VentoyGUI.loongarch64"
