#!/bin/sh
# Build the USB initramfs tools from one source tree per component.
# Usage: build/build-user.sh [x86_64|aarch64|loongarch64|mips64el|i386]...
# With no arguments, build every architecture whose compiler is installed.
# mips64el on Ubuntu is built by running this same script inside Debian.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/build/versions.env"

CACHE="$ROOT/.cache/upstream"
WORK="$ROOT/build/work/user"

need() {
    name=$1
    url=$2
    if [ ! -s "$CACHE/$name" ]; then
        wget -O "$CACHE/$name.part" "$url"
        mv "$CACHE/$name.part" "$CACHE/$name"
    fi
}

fetch() {
    mkdir -p "$CACHE"
    if [ ! -f "$CACHE/busybox-${BUSYBOX_VERSION}.tar.bz2" ]; then
        wget -O "$CACHE/busybox-${BUSYBOX_VERSION}.tar.bz2" "$BUSYBOX_URL"
    fi
    if [ ! -d "$CACHE/busybox-${BUSYBOX_VERSION}" ]; then
        tar -xf "$CACHE/busybox-${BUSYBOX_VERSION}.tar.bz2" -C "$CACHE"
    fi
    if [ ! -f "$CACHE/zstd-${ZSTD_VERSION}.tar.gz" ]; then
        wget -O "$CACHE/zstd-${ZSTD_VERSION}.tar.gz" "$ZSTD_URL"
    fi
    if [ ! -d "$CACHE/zstd-${ZSTD_VERSION}" ]; then
        tar -xf "$CACHE/zstd-${ZSTD_VERSION}.tar.gz" -C "$CACHE"
    fi
    if [ ! -f "$CACHE/zlib-${ZLIB_VERSION}.tar.gz" ]; then
        wget -O "$CACHE/zlib-${ZLIB_VERSION}.tar.gz" "$ZLIB_URL"
    fi
    if [ ! -f "$CACHE/fuse-${FUSE_VERSION}.tar.gz" ]; then
        wget -O "$CACHE/fuse-${FUSE_VERSION}.tar.gz" "$FUSE_URL"
    fi
    if [ ! -f "$CACHE/lvm2-${LVM2_VERSION}.tar.gz" ]; then
        wget -O "$CACHE/lvm2-${LVM2_VERSION}.tar.gz" "$LVM2_URL"
    fi
    if [ ! -f "$CACHE/xz-embedded-${XZ_EMBEDDED_VERSION}.tar.gz" ]; then
        wget -O "$CACHE/xz-embedded-${XZ_EMBEDDED_VERSION}.tar.gz" "$XZ_EMBEDDED_URL"
    fi
    need "squashfs-tools-${SQUASHFS_VERSION}.tar.gz" "$SQUASHFS_URL"
    need "lz4-${LZ4_VERSION}.tar.gz" "$LZ4_URL"
    need "lzo-${LZO_VERSION}.tar.gz" "$LZO_URL"
    need "xz-${XZ_VERSION}.tar.gz" "$XZ_URL"
    need "zstd-${ZSTD_LIB_VERSION}.tar.gz" "$ZSTD_LIB_URL"
    need "lunzip-${LUNZIP_VERSION}.tar.gz" "$LUNZIP_URL"
    need "smallz4-${SMALLZ4_VERSION}.tar.gz" "$SMALLZ4_URL"
    need "vblade-${VBLADE_VERSION}.tar.gz" "$VBLADE_URL"
}

arch_cc() {
    case "$1" in
        x86_64) echo gcc ;;
        aarch64) echo aarch64-linux-gnu-gcc ;;
        loongarch64) echo loongarch64-linux-gnu-gcc ;;
        mips64el) echo mips64el-linux-gnuabi64-gcc ;;
        i386) echo i686-linux-gnu-gcc ;;
        *) echo "unknown arch $1" >&2; return 1 ;;
    esac
}

arch_cross() {
    case "$1" in
        x86_64) echo "" ;;
        *)
            cc=$(arch_cc "$1")
            echo "${cc%gcc}"
            ;;
    esac
}

arch_host() {
    case "$1" in
        x86_64) echo x86_64-linux-gnu ;;
        aarch64) echo aarch64-linux-gnu ;;
        loongarch64) echo loongarch64-linux-gnu ;;
        mips64el) echo mips64el-linux-gnuabi64 ;;
        i386) echo i686-linux-gnu ;;
    esac
}

arch_busybox() {
    case "$1" in
        x86_64) echo x86_64 ;;
        aarch64) echo arm64 ;;
        loongarch64) echo loongarch64 ;;
        mips64el) echo mips ;;
        i386) echo i386 ;;
    esac
}

arch_cflags() {
    case "$1" in
        mips64el) echo "-mabi=64" ;;
        *) echo "" ;;
    esac
}

have_cc() {
    command -v "$(arch_cc "$1")" >/dev/null 2>&1
}

build_simple() {
    (
        cd "$ROOT/VtoyTool"
        # shellcheck disable=SC2086
        "$CC" -Os -static $CFLAGS -D_FILE_OFFSET_BITS=64 ./*.c BabyISO/*.c -IBabyISO -Wall \
            -DBUILD_VTOY_TOOL -o "$OUT/vtoytool"
    )
    (
        vdir="$WORK/$ARCH/vblade-${VBLADE_VERSION}"
        rm -rf "$vdir"
        mkdir -p "$vdir"
        tar -xf "$CACHE/vblade-${VBLADE_VERSION}.tar.gz" -C "$vdir" --strip-components=1
        patch -p1 -d "$vdir" < "$ROOT/build/patches/vblade/0001-ventoy-vblade.patch"
        cd "$vdir"
        # vblade relies on tentative definitions. GCC 10 defaults to -fno-common.
        # shellcheck disable=SC2086
        "$CC" -Os -static -fcommon $CFLAGS linux.c aoe.c ata.c bpf.c -o "$OUT/vblade"
    )
    (
        sdir="$WORK/$ARCH/smallz4-${SMALLZ4_VERSION}"
        rm -rf "$sdir"
        mkdir -p "$sdir"
        tar -xf "$CACHE/smallz4-${SMALLZ4_VERSION}.tar.gz" -C "$sdir" --strip-components=1
        # shellcheck disable=SC2086
        "$CC" -Os -static $CFLAGS -D_FILE_OFFSET_BITS=64 "$sdir/smallz4cat.c" -o "$OUT/lz4cat"
    )
    build_xzminidec
    ldir="$WORK/$ARCH/lunzip"
    rm -rf "$ldir"
    mkdir -p "$ldir"
    tar -xf "$CACHE/lunzip-${LUNZIP_VERSION}.tar.gz" -C "$WORK/$ARCH"
    (
        cd "$WORK/$ARCH/lunzip-${LUNZIP_VERSION}"
        # shellcheck disable=SC2086
        ./configure CC="$CC -static $CFLAGS"
        make -j"$(nproc)"
        cp -a lunzip "$OUT/lunzip"
    )
}

build_xzminidec() {
    src="$WORK/$ARCH/xz-embedded-${XZ_EMBEDDED_VERSION}"
    rm -rf "$src"
    mkdir -p "$src"
    tar -xf "$CACHE/xz-embedded-${XZ_EMBEDDED_VERSION}.tar.gz" -C "$src" --strip-components=1
    (
        cd "$src/userspace"
        # Upstream's Makefile sets CC to "gcc -std=gnu11". Override the whole value.
        # shellcheck disable=SC2086
        make -j"$(nproc)" xzminidec \
            CC="$CC -Os -static -std=gnu11 $CFLAGS" \
            CFLAGS="-O2 -Wall -Wextra"
        cp -a xzminidec "$OUT/xzminidec"
    )
}

build_zstdcat() {
    zdir="$WORK/$ARCH/zstd"
    rm -rf "$zdir"
    cp -a "$CACHE/zstd-${ZSTD_VERSION}" "$zdir"
    # shellcheck disable=SC2086
    make -C "$zdir/lib" -j"$(nproc)" libzstd.a CC="$CC" CFLAGS="-O3 $CFLAGS" MOREFLAGS=
    # shellcheck disable=SC2086
    "$CC" -O3 -static $CFLAGS \
        -I"$zdir/lib" -I"$zdir/lib/common" -I"$zdir/programs" \
        "$zdir/programs/zstdcli.c" "$zdir/programs/util.c" \
        "$zdir/programs/timefn.c" "$zdir/programs/fileio.c" \
        "$zdir/programs/fileio_asyncio.c" "$zdir/programs/benchfn.c" \
        "$zdir/programs/benchzstd.c" "$zdir/programs/datagen.c" \
        "$zdir/programs/dibio.c" "$zdir/programs/lorem.c" "$zdir/programs/zstdcli_trace.c" \
        "$zdir/lib/libzstd.a" -o "$OUT/zstdcat"
}

build_busybox() {
    bdir="$WORK/$ARCH/busybox"
    rm -rf "$bdir"
    mkdir -p "$bdir"
    # Migrated from BUSYBOX/aarch64_ash.config onto BusyBox 1.36.1. Same file for every arch.
    cp "$ROOT/BUSYBOX/ash-1.36.config" "$bdir/.config"
    bb="$CACHE/busybox-${BUSYBOX_VERSION}"
    # shellcheck disable=SC2086
    make -C "$bb" O="$bdir" -j"$(nproc)" CROSS_COMPILE="$CROSS" ARCH="$BBARCH" \
        CFLAGS="-Os $CFLAGS" LDFLAGS="-static $CFLAGS"
    cp -a "$bdir/busybox" "$OUT/ash"
}

refresh_autotools() {
    dir=$1
    sub="$ROOT/build/autotools/config.sub"
    guess="$ROOT/build/autotools/config.guess"
    if [ ! -f "$sub" ] || [ ! -f "$guess" ]; then
        echo "build/autotools/config.sub is missing" >&2
        return 1
    fi
    find "$dir" -name config.sub -exec cp "$sub" {} \;
    find "$dir" -name config.guess -exec cp "$guess" {} \;
}

build_unsquashfs() {
    p="$WORK/$ARCH/prefix"
    rm -rf "$p" "$WORK/$ARCH/comp"
    mkdir -p "$WORK/$ARCH/comp" "$p"

    rm -rf "$WORK/$ARCH/zlib-${ZLIB_VERSION}"
    tar -xf "$CACHE/zlib-${ZLIB_VERSION}.tar.gz" -C "$WORK/$ARCH"
    (
        cd "$WORK/$ARCH/zlib-${ZLIB_VERSION}"
        # shellcheck disable=SC2086
        CC="$CC" CFLAGS="-O2 $CFLAGS" ./configure --static --prefix="$p"
        make -j"$(nproc)"
        make install
    )

    tar -xf "$CACHE/lz4-${LZ4_VERSION}.tar.gz" -C "$WORK/$ARCH/comp"
    # shellcheck disable=SC2086
    make -C "$WORK/$ARCH/comp/lz4-${LZ4_VERSION}" -j"$(nproc)" \
        CC="$CC" CFLAGS="-O2 $CFLAGS" BUILD_SHARED=no PREFIX="$p" install

    tar -xf "$CACHE/lzo-${LZO_VERSION}.tar.gz" -C "$WORK/$ARCH/comp"
    refresh_autotools "$WORK/$ARCH/comp/lzo-${LZO_VERSION}"
    (
        cd "$WORK/$ARCH/comp/lzo-${LZO_VERSION}"
        # shellcheck disable=SC2086
        ./configure --host="$HOSTT" --prefix="$p" --disable-shared --enable-static \
            CC="$CC" CFLAGS="-O2 $CFLAGS"
        make -j"$(nproc)"
        make install
    )

    rm -rf "$WORK/$ARCH/comp/xz-${XZ_VERSION}"
    tar -xf "$CACHE/xz-${XZ_VERSION}.tar.gz" -C "$WORK/$ARCH/comp"
    refresh_autotools "$WORK/$ARCH/comp/xz-${XZ_VERSION}"
    chmod +x "$WORK/$ARCH/comp/xz-${XZ_VERSION}/configure"
    (
        cd "$WORK/$ARCH/comp/xz-${XZ_VERSION}"
        # shellcheck disable=SC2086
        ./configure --host="$HOSTT" --prefix="$p" --disable-shared --enable-static \
            --disable-xz --disable-xzdec --disable-lzmadec --disable-lzmainfo --enable-small --disable-nls \
            CC="$CC" CFLAGS="-O2 $CFLAGS"
        make -j"$(nproc)"
        make install
    )

    rm -rf "$WORK/$ARCH/comp/zstd-${ZSTD_LIB_VERSION}"
    tar -xf "$CACHE/zstd-${ZSTD_LIB_VERSION}.tar.gz" -C "$WORK/$ARCH/comp"
    # shellcheck disable=SC2086
    make -C "$WORK/$ARCH/comp/zstd-${ZSTD_LIB_VERSION}/lib" -j"$(nproc)" libzstd.a \
        CC="$CC" CFLAGS="-O2 $CFLAGS" ZSTD_LIB_COMPRESSION=0
    mkdir -p "$p/include" "$p/lib"
    cp -a "$WORK/$ARCH/comp/zstd-${ZSTD_LIB_VERSION}/lib/libzstd.a" "$p/lib/"
    cp -a "$WORK/$ARCH/comp/zstd-${ZSTD_LIB_VERSION}/lib/zstd.h" \
        "$WORK/$ARCH/comp/zstd-${ZSTD_LIB_VERSION}/lib/common/zstd_errors.h" "$p/include/"

    sq="$WORK/$ARCH/squashfs-tools-${SQUASHFS_VERSION}"
    rm -rf "$sq"
    mkdir -p "$sq"
    tar -xf "$CACHE/squashfs-tools-${SQUASHFS_VERSION}.tar.gz" -C "$sq" --strip-components=1
    patch -p1 -d "$sq" < "$ROOT/build/patches/squashfs/0001-ventoy-squashfs.patch"
    # shellcheck disable=SC2086
    make -C "$sq/squashfs-tools" -j"$(nproc)" unsquashfs \
        CC="$CC" \
        LDFLAGS="-static $CFLAGS" \
        EXTRA_CFLAGS="$CFLAGS" \
        VTZLIB="$p/lib/libz.a" \
        LZMA_LIBDIR="$p" \
        LZO_LIBDIR="$p" \
        LZ4_LIBDIR="$p" \
        ZSTD_LIBDIR="$p"
    cp -a "$sq/squashfs-tools/unsquashfs" "$OUT/unsquashfs"
}

build_fuse() {
    src="$WORK/$ARCH/fuse-${FUSE_VERSION}"
    if [ ! -d "$src" ]; then
        tar -xf "$CACHE/fuse-${FUSE_VERSION}.tar.gz" -C "$WORK/$ARCH"
    fi
    if grep -q '#define __u64 uint64_t' "$src/include/fuse_kernel.h"; then
        patch -p1 -d "$src" < "$ROOT/build/patches/fuse/0001-use-linux-types.patch"
    fi
    if [ ! -f "$src/lib/.libs/libfuse.a" ]; then
        refresh_autotools "$src"
        (
            cd "$src"
            ./configure --host="$HOSTT" CC="$CC" \
                --enable-static --disable-shared \
                --disable-example --disable-mtab --disable-util
            make -C lib
        )
    fi
    lib="$src/lib/.libs/libfuse.a"
    if [ ! -f "$lib" ]; then
        lib="$src/lib/libfuse.a"
    fi
    "$CC" -static -Os -D_FILE_OFFSET_BITS=64 $CFLAGS \
        -I"$src/include" \
        "$ROOT/FUSEISO/vtoy_fuse_iso.c" \
        -o "$OUT/vtoy_fuse_iso" \
        "$lib" -lpthread -ldl
}

build_dmsetup() {
    src="$WORK/$ARCH/lvm2-${LVM2_VERSION}"
    if [ ! -x "$src/libdm/dm-tools/dmsetup.static" ]; then
        rm -rf "$src"
        tar -xf "$CACHE/lvm2-${LVM2_VERSION}.tar.gz" -C "$WORK/$ARCH"
        refresh_autotools "$src"
        (
            cd "$src"
            ./configure --host="$HOSTT" CC="$CC" \
                --prefix="$WORK/$ARCH/prefix" \
                --enable-static_link \
                --disable-selinux \
                --disable-nls \
                --disable-readline \
                --disable-udev_sync \
                --disable-udev_rules \
                --disable-blkid_wiping \
                --disable-fsadm \
                --disable-cmdlib \
                --disable-lvmpolld \
                --disable-use_lvmpolld \
                --disable-dmeventd \
                --disable-python3_bindings \
                --disable-dbus-service
            make -C libdm
            make -C libdm/dm-tools dmsetup.static
        )
    fi
    cp -a "$src/libdm/dm-tools/dmsetup.static" "$OUT/dmsetup"
}

build_arch() {
    ARCH=$1
    CC=$(arch_cc "$ARCH")
    CROSS=$(arch_cross "$ARCH")
    HOSTT=$(arch_host "$ARCH")
    BBARCH=$(arch_busybox "$ARCH")
    CFLAGS=$(arch_cflags "$ARCH")
    STRIP=${CROSS}strip
    if [ "$ARCH" = "x86_64" ]; then
        STRIP=strip
    fi
    if ! command -v "$CC" >/dev/null 2>&1; then
        echo "skip $ARCH: $CC is not installed" >&2
        return 0
    fi
    OUT="$ROOT/build/out/tools/$ARCH"
    mkdir -p "$OUT" "$WORK/$ARCH"
    echo "======== $ARCH ($CC) ========"
    if [ "${VENTOY_ONLY:-}" = xzminidec ]; then
        build_xzminidec
        "$STRIP" --strip-all "$OUT/xzminidec"
        if command -v file >/dev/null 2>&1; then
            file "$OUT/xzminidec"
        fi
        return 0
    fi
    build_simple
    build_zstdcat
    build_busybox
    build_unsquashfs
    build_fuse
    build_dmsetup
    "$STRIP" --strip-all "$OUT"/*
    if command -v file >/dev/null 2>&1; then
        file "$OUT"/*
    fi
}

fetch

if [ "$#" -eq 0 ]; then
    set --
    for arch in x86_64 aarch64 loongarch64 mips64el i386; do
        if have_cc "$arch"; then
            set -- "$@" "$arch"
        fi
    done
fi

for arch in "$@"; do
    build_arch "$arch"
done
