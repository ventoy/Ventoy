#!/bin/sh
# Build Ventoy's GRUB from the pinned upstream tarball.
# Usage: build/build-grub.sh [x86_64-efi|i386-efi|arm64-efi|loongarch64-efi|mips64el-efi|i386-pc]...
# With no arguments, build every target whose cross compiler is installed.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/build/versions.env"
. "$ROOT/build/grub-module-sets.sh"

CACHE="$ROOT/.cache/upstream"
WORK="$ROOT/build/work/grub"
OUT="$ROOT/build/out"

fetch() {
    mkdir -p "$CACHE"
    if [ ! -f "$CACHE/grub-${GRUB_VERSION}.tar.xz" ]; then
        wget -O "$CACHE/grub-${GRUB_VERSION}.tar.xz" "$GRUB_URL"
    fi
    if [ ! -d "$CACHE/grub-${GRUB_VERSION}" ]; then
        tar -xf "$CACHE/grub-${GRUB_VERSION}.tar.xz" -C "$CACHE"
    fi
    if [ ! -d "$CACHE/grub-2.04" ]; then
        if [ ! -f "$CACHE/grub-2.04.tar.xz" ]; then
            wget -O "$CACHE/grub-2.04.tar.xz" https://ftp.gnu.org/gnu/grub/grub-2.04.tar.xz
        fi
        tar -xf "$CACHE/grub-2.04.tar.xz" -C "$CACHE"
    fi
}

have_cc() {
    command -v "$1" >/dev/null 2>&1
}

configure_one() {
    target=$1
    prefix=$2
    extra=$3
    build=$WORK/build-$target
    rm -rf "$build"
    mkdir -p "$build"
    (
        cd "$build"
        # GCC 15 rejects const-discarding host assignments that GRUB 2.14 still has.
        # -Os has to be here: configure adds it only when TARGET_CFLAGS is empty.
        export HOST_CFLAGS="-Wno-error=discarded-qualifiers -Wno-error=misleading-indentation ${HOST_CFLAGS:-}"
        export TARGET_CFLAGS="-Os -Wno-error=discarded-qualifiers -Wno-error=misleading-indentation ${TARGET_CFLAGS:-}"
        # shellcheck disable=SC2086
        "$WORK/configure" --with-platform="$(printf '%s' "$target" | sed 's/.*-//')" \
            --target="$(printf '%s' "$target" | sed 's/-.*//')" \
            --prefix="$OUT/prefix-$target" \
            --host=x86_64-linux-gnu \
            HOST_CC=gcc \
            BUILD_CC=gcc \
            $extra
        # Current bfd accepts --image-base but places .text 0x74 bytes past
        # the requested 0x9000, so grub-mkimage rejects kernel.img.
        if [ "$target" = "i386-pc" ]; then
            sed -i 's/^TARGET_IMG_BASE_LDOPT = -Wl,--image-base$/TARGET_IMG_BASE_LDOPT = -Wl,-Ttext/' grub-core/Makefile
        fi
        # MIPS gcc warns on Ventoy's unaligned casts, and GRUB appends -Werror
        # after TARGET_CFLAGS, so the waiver has to be added on the make line.
        if [ "$target" = "mips64el-efi" ]; then
            find . -name Makefile -exec sed -i \
                '/^TARGET_CFLAGS = /{/-Wno-error=cast-align/!s/$/ -Wno-error=cast-align -Wno-error=address-of-packed-member/}' {} +
        fi
        make -j"$(nproc)"
        make install
    )
    prefix_bin="$OUT/prefix-$target"
    mkdir -p "$OUT/images"
    case "$target" in
        x86_64-efi)
            mods=$(grub_embed_mods x86_64-efi)
            # Firmware loads EFI/BOOT/BOOTX64.EFI. That name stays the
            # Microsoft-signed Rocky shim shipped in INSTALL/EFI/BOOT.
            # GRUB is grubx64_real.efi; fbx64.efi checks its hash and chainloads it.
            mkdir -p "$ROOT/INSTALL/EFI/BOOT" "$OUT/images"
            "$prefix_bin/bin/grub-mkimage" -O x86_64-efi -p '(,2)/grub' --compression auto \
                -o "$ROOT/INSTALL/EFI/BOOT/grubx64_real.efi" $mods
            cp -a "$ROOT/INSTALL/EFI/BOOT/grubx64_real.efi" "$OUT/images/grubx64_real.efi"
            live_mods="search iso9660 fat part_msdos part_gpt configfile normal linux echo test gzio"
            "$prefix_bin/bin/grub-mkimage" -O x86_64-efi -p '/EFI/boot' \
                -c "$ROOT/LiveCDGUI/GRUB/embed.cfg" \
                -o "$ROOT/LiveCDGUI/GRUB/bootx64.efi" $live_mods
            cp -a "$ROOT/LiveCDGUI/GRUB/bootx64.efi" "$ROOT/LiveCD/GRUB/bootx64.efi"
            ;;
        i386-efi)
            mods=$(grub_embed_mods i386-efi)
            # Firmware loads EFI/BOOT/BOOTIA32.EFI. That name stays the
            # Super-UEFIinSecureBoot-Disk shim shipped in INSTALL/EFI/BOOT.
            # The shim loads grubia32.efi, which chainloads grubia32_real.efi.
            mkdir -p "$ROOT/INSTALL/EFI/BOOT" "$OUT/images"
            "$prefix_bin/bin/grub-mkimage" -O i386-efi -p '(,2)/grub' --compression auto \
                -o "$ROOT/INSTALL/EFI/BOOT/grubia32_real.efi" $mods
            cp -a "$ROOT/INSTALL/EFI/BOOT/grubia32_real.efi" "$OUT/images/grubia32_real.efi"
            ;;
        arm64-efi)
            mods=$(grub_embed_mods arm64-efi)
            "$prefix_bin/bin/grub-mkimage" -O arm64-efi -p '(,2)/grub' --compression auto \
                -o "$OUT/images/BOOTAA64.EFI" $mods
            mkdir -p "$ROOT/INSTALL/EFI/BOOT"
            cp -a "$OUT/images/BOOTAA64.EFI" "$ROOT/INSTALL/EFI/BOOT/BOOTAA64.EFI"
            ;;
        loongarch64-efi)
            mods=$(grub_embed_mods loongarch64-efi)
            "$prefix_bin/bin/grub-mkimage" -O loongarch64-efi -p '(,2)/grub' --compression auto \
                -o "$OUT/images/BOOTLOONGARCH64.EFI" $mods
            ;;
        mips64el-efi)
            mods=$(grub_embed_mods mips64el-efi)
            "$prefix_bin/bin/grub-mkimage" -O mips64el-efi -p '(,2)/grub' --compression auto \
                -o "$OUT/images/BOOTMIPS.EFI" $mods
            ;;
        i386-pc)
            mods=$(grub_embed_mods i386-pc)
            "$prefix_bin/bin/grub-mkimage" -O i386-pc -p '(,2)/grub' --compression auto \
                -o "$OUT/images/core.img" $mods
            live_mods="biosdisk iso9660 fat part_msdos configfile normal linux linux16 echo test search"
            "$prefix_bin/bin/grub-mkimage" -O i386-pc-eltorito -p '/EFI/boot' \
                -c "$ROOT/LiveCDGUI/GRUB/embed.cfg" \
                -o "$ROOT/LiveCDGUI/GRUB/cdrom.img" $live_mods
            cp -a "$prefix_bin/lib/grub/i386-pc/boot_hybrid.img" "$ROOT/LiveCDGUI/GRUB/boot_hybrid.img"
            cp -a "$ROOT/LiveCDGUI/GRUB/cdrom.img" "$ROOT/LiveCD/GRUB/cdrom.img"
            cp -a "$ROOT/LiveCDGUI/GRUB/boot_hybrid.img" "$ROOT/LiveCD/GRUB/boot_hybrid.img"
            ;;
    esac
}

fetch
if [ ! -f "$WORK/configure.ac" ]; then
    rm -rf "$WORK"
    mkdir -p "$WORK"
    cp -a "$CACHE/grub-${GRUB_VERSION}/." "$WORK/"
    "$ROOT/build/apply-patches.sh" grub "$WORK"
fi
# GRUB 2.14's release makefiles were generated by automake 1.16.5 and call
# that exact name. A newer system automake refuses the version stamp.
ensure_automake_116() {
    am="$ROOT/build/work/automake-1.16.5/bin/automake"
    if [ ! -x "$am" ]; then
        mkdir -p "$CACHE"
        if [ ! -f "$CACHE/automake-1.16.5.tar.xz" ]; then
            wget -O "$CACHE/automake-1.16.5.tar.xz" https://ftp.gnu.org/gnu/automake/automake-1.16.5.tar.xz
        fi
        rm -rf "$ROOT/build/work/automake-src"
        tar -xf "$CACHE/automake-1.16.5.tar.xz" -C "$ROOT/build/work"
        mv "$ROOT/build/work/automake-1.16.5" "$ROOT/build/work/automake-src"
        (
            cd "$ROOT/build/work/automake-src"
            ./configure --prefix="$ROOT/build/work/automake-1.16.5"
            make -j"$(nproc)"
            make install
        )
    fi
    mkdir -p "$ROOT/build/work/bin"
    ln -sfn "$ROOT/build/work/automake-1.16.5/bin/automake" "$ROOT/build/work/bin/automake-1.16"
    ln -sfn "$ROOT/build/work/automake-1.16.5/bin/aclocal" "$ROOT/build/work/bin/aclocal-1.16"
    export PATH="$ROOT/build/work/bin:$PATH"
}
ensure_automake_116
(
    cd "$WORK"
    python3 gentpl.py Makefile.util.def Makefile.utilgcry.def > Makefile.util.am
    python3 gentpl.py grub-core/Makefile.core.def grub-core/Makefile.gcry.def > grub-core/Makefile.core.am
)
# Patches and gentpl.py are newer than the release's generated files.
# Make would otherwise rerun aclocal, which stops because autoconf-archive
# is not installed. Patch 0002 already carries configure and Makefile.in.
find "$WORK" -type f \( \
    -name Makefile.in -o -name aclocal.m4 -o -name configure \
    -o -name config.h.in -o -name config-util.h.in \
    \) -exec touch {} +

if [ "$#" -eq 0 ]; then
    set -- x86_64-efi
    have_cc aarch64-linux-gnu-gcc && set -- "$@" arm64-efi
    have_cc loongarch64-linux-gnu-gcc && set -- "$@" loongarch64-efi
    if have_cc mips64el-linux-gnuabi64-gcc || have_cc mips-linux-gnu-gcc; then
        set -- "$@" mips64el-efi
    fi
    have_cc i686-linux-gnu-gcc && set -- "$@" i386-efi i386-pc
fi

for target in "$@"; do
    case "$target" in
        x86_64-efi)
            configure_one x86_64-efi "" ""
            ;;
        i386-efi)
            configure_one i386-efi "" "TARGET_CC=i686-linux-gnu-gcc TARGET_OBJCOPY=i686-linux-gnu-objcopy TARGET_STRIP=i686-linux-gnu-strip TARGET_NM=i686-linux-gnu-nm TARGET_RANLIB=i686-linux-gnu-ranlib"
            ;;
        arm64-efi)
            configure_one arm64-efi "" "TARGET_CC=aarch64-linux-gnu-gcc TARGET_OBJCOPY=aarch64-linux-gnu-objcopy TARGET_STRIP=aarch64-linux-gnu-strip TARGET_NM=aarch64-linux-gnu-nm TARGET_RANLIB=aarch64-linux-gnu-ranlib"
            ;;
        loongarch64-efi)
            configure_one loongarch64-efi "" "TARGET_CC=loongarch64-linux-gnu-gcc TARGET_OBJCOPY=loongarch64-linux-gnu-objcopy TARGET_STRIP=loongarch64-linux-gnu-strip TARGET_NM=loongarch64-linux-gnu-nm TARGET_RANLIB=loongarch64-linux-gnu-ranlib"
            ;;
        mips64el-efi)
            if have_cc mips64el-linux-gnuabi64-gcc; then
                triple=mips64el-linux-gnuabi64
            elif have_cc mips-linux-gnu-gcc; then
                triple=mips-linux-gnu
            else
                echo "mips64el-efi needs mips64el-linux-gnuabi64-gcc or mips-linux-gnu-gcc" >&2
                exit 1
            fi
            configure_one mips64el-efi "" "TARGET_CC=${triple}-gcc TARGET_OBJCOPY=${triple}-objcopy TARGET_STRIP=${triple}-strip TARGET_NM=${triple}-nm TARGET_RANLIB=${triple}-ranlib"
            ;;
        i386-pc)
            configure_one i386-pc "" "TARGET_CC=i686-linux-gnu-gcc TARGET_OBJCOPY=i686-linux-gnu-objcopy TARGET_STRIP=i686-linux-gnu-strip TARGET_NM=i686-linux-gnu-nm TARGET_RANLIB=i686-linux-gnu-ranlib"
            ;;
        *)
            echo "unknown target $target" >&2
            exit 1
            ;;
    esac
done

echo "images in $OUT/images"
ls -l "$OUT/images"
