#!/bin/sh
# Full static BusyBox for the initramfs hook, plus the tiny chmod helper.
# The ash binary from build-user.sh is only a shell. The hook scripts need
# grep, cp, mount and the rest, which this defconfig build provides.
# Usage: build/build-init-busybox.sh [x86_64|i386|aarch64|loongarch64|mips64el]...

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/build/versions.env"

CACHE="$ROOT/.cache/upstream"
SRC="$CACHE/busybox-${BUSYBOX_VERSION}"

if [ ! -d "$SRC" ]; then
    echo "busybox source is not unpacked in .cache/upstream" >&2
    exit 1
fi

build_one() {
    arch=$1
    cross=$2
    bbarch=$3
    out="$ROOT/build/out/initramfs/$arch"
    work="$ROOT/build/work/busybox-full-$arch"
    rm -rf "$work"
    mkdir -p "$work" "$out"
    make -C "$SRC" O="$work" defconfig
    sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' "$work/.config"
    if ! grep -q '^CONFIG_STATIC=y$' "$work/.config"; then
        echo 'CONFIG_STATIC=y' >> "$work/.config"
    fi
    # Current kernel headers no longer ship the CBQ traffic-control types.
    sed -i \
        -e 's/^CONFIG_TC=y/# CONFIG_TC is not set/' \
        -e 's/^CONFIG_FEATURE_TC_.*=y/# &/' \
        "$work/.config"
    yes '' | make -C "$SRC" O="$work" oldconfig >/dev/null
    make -C "$SRC" O="$work" -j"$(nproc)" ARCH="$bbarch" CROSS_COMPILE="$cross"
    cp -a "$work/busybox" "$out/busybox"
    xz -c -C crc32 -9 "$out/busybox" > "$out/busybox.xz"
    "${cross}gcc" -static -Os "$ROOT/BUSYBOX/chmod/vtchmod.c" -o "$out/vtchmod"
    "${cross}strip" --strip-all "$out/vtchmod" || true
    chmod 755 "$out/busybox" "$out/vtchmod"
    echo "init busybox $arch -> $out"
}

if [ "$#" -eq 0 ]; then
    set -- x86_64 aarch64 loongarch64
    command -v i686-linux-gnu-gcc >/dev/null 2>&1 && set -- "$@" i386
    command -v mips64el-linux-gnuabi64-gcc >/dev/null 2>&1 && set -- "$@" mips64el
fi

for arch in "$@"; do
    case "$arch" in
        x86_64) build_one x86_64 "" x86_64 ;;
        i386) build_one i386 i686-linux-gnu- i386 ;;
        aarch64) build_one aarch64 aarch64-linux-gnu- arm64 ;;
        loongarch64) build_one loongarch64 loongarch64-linux-gnu- loongarch64 ;;
        mips64el) build_one mips64el mips64el-linux-gnuabi64- mips64 ;;
        *) echo "unknown arch $arch" >&2; exit 1 ;;
    esac
done
