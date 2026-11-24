#!/bin/sh
# Build BOOTMIPS.EFI inside Debian, which ships gcc-mips64el-linux-gnuabi64.
# Ubuntu's archive does not. The GRUB tree must already be prepared.
# Usage: build/build-mips64el-docker.sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)

if [ "${1:-}" != "--inside" ]; then
    docker build -t ventoy-grub-mips64el -f "$ROOT/build/Dockerfile.mips64el" "$ROOT/build"
    exec docker run --rm \
        -v "$ROOT":/src \
        -w /src \
        -e HOST_UID="$(id -u)" \
        -e HOST_GID="$(id -g)" \
        ventoy-grub-mips64el \
        bash /src/build/build-mips64el-docker.sh --inside
fi

chown_back() {
    if [ -n "${HOST_UID:-}" ]; then
        chown -R "$HOST_UID:$HOST_GID" \
            /src/build/work/grub/build-mips64el-efi \
            /src/build/out/prefix-mips64el-efi \
            /src/build/out/images/BOOTMIPS.EFI \
            2>/dev/null || true
    fi
}
trap chown_back EXIT

WORK=/src/build/work/grub
OUT=/src/build/out
# The prepared tree already has configure. Do not rerun autoconf: this image
# does not carry autoconf-archive, and a regen would drop the mips64el edits.
build=$WORK/build-mips64el-efi
rm -rf "$build"
mkdir -p "$build"
cd "$build"
export HOST_CFLAGS="-Wno-error=discarded-qualifiers -Wno-error=misleading-indentation ${HOST_CFLAGS:-}"
# MIPS gcc rejects Ventoy's existing unaligned casts. x86 and LoongArch already build this code.
export TARGET_CFLAGS="-Wno-error=discarded-qualifiers -Wno-error=misleading-indentation -Wno-error=cast-align ${TARGET_CFLAGS:-}"
"$WORK/configure" \
    --with-platform=efi \
    --target=mips64el \
    --prefix="$OUT/prefix-mips64el-efi" \
    --host=x86_64-linux-gnu \
    HOST_CC=gcc \
    BUILD_CC=gcc \
    TARGET_CC=mips64el-linux-gnuabi64-gcc \
    TARGET_OBJCOPY=mips64el-linux-gnuabi64-objcopy \
    TARGET_STRIP=mips64el-linux-gnuabi64-strip \
    TARGET_NM=mips64el-linux-gnuabi64-nm \
    TARGET_RANLIB=mips64el-linux-gnuabi64-ranlib
# Keep generated files newer than configure.ac. autoheader and automake-1.16
# are not available here, and rerunning them would need autoconf-archive.
touch "$WORK/configure.ac" "$WORK/aclocal.m4" \
    "$WORK/Makefile.am" "$WORK/grub-core/Makefile.am" "$WORK/grub-core/Makefile.core.am"
sleep 1
touch "$WORK/configure" "$WORK/Makefile.in" "$WORK/grub-core/Makefile.in" \
    "$WORK/config.h.in" "$WORK/config-util.h.in"
sleep 1
touch ./config.status ./Makefile ./grub-core/Makefile \
    ./config.h ./config-util.h ./stamp-h ./stamp-h1
# -Werror is appended after TARGET_CFLAGS, so the waiver has to follow it.
find . -name Makefile -print0 | xargs -0 sed -i \
    '/^TARGET_CFLAGS = /{/-Wno-error=cast-align/!s/$/ -Wno-error=cast-align -Wno-error=address-of-packed-member/}'
make -j"$(nproc)"
make install

mkdir -p "$OUT/images"
mods="part_gpt part_msdos fat exfat ntfs ext2 xfs iso9660 udf squash4 loopback linux normal configfile test echo search gzio xzio ventoy"
"$OUT/prefix-mips64el-efi/bin/grub-mkimage" \
    -O mips64el-efi -p '(,2)/grub' \
    -o "$OUT/images/BOOTMIPS.EFI" \
    $mods
mips64el-linux-gnuabi64-objdump -f "$OUT/images/BOOTMIPS.EFI" | head -n 5
ls -l "$OUT/images/BOOTMIPS.EFI"
