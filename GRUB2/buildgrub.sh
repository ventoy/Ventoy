#!/bin/bash

set -e

VT_GRUB_DIR=$PWD
VT_SRC_VER=grub-2.06
VT_SRC_TAR=$VT_GRUB_DIR/$VT_SRC_VER.tar.xz
VT_TARGET=${1:-all}
VT_BUILD_ROOT=${VT_BUILD_ROOT:-$(mktemp -d /tmp/ventoy-grub206.XXXXXX)}
VT_SRC_DIR=$VT_BUILD_ROOT/src
VT_ARM64_GCC_DIR=${VT_ARM64_GCC_DIR:-/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu}
VT_ARM64_GCC_TAR=${VT_ARM64_GCC_TAR:-/opt/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz}
VT_ARM64_GCC_URL=${VT_ARM64_GCC_URL:-https://developer.arm.com/-/media/Files/downloads/gnu/11.3.rel1/binrel/arm-gnu-toolchain-11.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz}
VT_ARM64_PREFIX=${VT_ARM64_PREFIX:-aarch64-none-linux-gnu-}
VT_ENABLE_MIPS64EL=${VT_ENABLE_MIPS64EL:-1}
VT_MIPS_TOOLCHAIN_DIR=${VT_MIPS_TOOLCHAIN_DIR:-/opt/mips-loongson-gcc7.3-linux-gnu/2019.06-29}
VT_MIPS_TOOLCHAIN_TAR=${VT_MIPS_TOOLCHAIN_TAR:-/opt/mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz}
VT_MIPS_TOOLCHAIN_URL=${VT_MIPS_TOOLCHAIN_URL:-https://github.com/ventoy/vtoytoolchain/releases/download/1.0/mips-loongson-gcc7.3-2019.06-29-linux-gnu.tar.gz}
VT_MIPS_PREFIX=${VT_MIPS_PREFIX:-mips-linux-gnu-}

if [ -z "${VT_HOST_OUT_ROOT:-}" ]; then
    VT_HOST_OUT_ROOT=$VT_GRUB_DIR/INSTALL
fi

cleanup() {
    if [ "${VT_KEEP_BUILD_ROOT:-0}" = "1" ]; then
        echo "======== KEEP BUILD ROOT: $VT_BUILD_ROOT ============="
        return
    fi

    rm -rf "$VT_BUILD_ROOT"
}

trap cleanup EXIT

prepare_arm64_toolchain() {
    if [ -x "$VT_ARM64_GCC_DIR/bin/${VT_ARM64_PREFIX}gcc" ]; then
        return
    fi

    if [ ! -f "$VT_ARM64_GCC_TAR" ]; then
        wget -q -O "$VT_ARM64_GCC_TAR" "$VT_ARM64_GCC_URL" || exit 1
    fi

    tar xf "$VT_ARM64_GCC_TAR" -C /opt || exit 1
}

prepare_mips_toolchain() {
    if [ -x "$VT_MIPS_TOOLCHAIN_DIR/bin/${VT_MIPS_PREFIX}gcc" ]; then
        return
    fi

    if [ ! -f "$VT_MIPS_TOOLCHAIN_TAR" ]; then
        wget -q -O "$VT_MIPS_TOOLCHAIN_TAR" "$VT_MIPS_TOOLCHAIN_URL" || exit 1
    fi

    tar xf "$VT_MIPS_TOOLCHAIN_TAR" -C /opt || exit 1
}

if ! command -v python >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1; then
    ln -sf "$(command -v python3)" /usr/local/bin/python
fi

rm -rf "$VT_HOST_OUT_ROOT"
mkdir -p "$VT_SRC_DIR" "$VT_HOST_OUT_ROOT"

if [ ! -f "$VT_SRC_TAR" ]; then
    wget -O "$VT_SRC_TAR" "https://ftp.gnu.org/gnu/grub/$VT_SRC_VER.tar.xz" || exit 1
fi

tar -xf "$VT_SRC_TAR" -C "$VT_SRC_DIR/" || exit 1
/bin/cp -a "$VT_GRUB_DIR/MOD_SRC/$VT_SRC_VER/." "$VT_SRC_DIR/$VT_SRC_VER/" || exit 1

cd "$VT_SRC_DIR/$VT_SRC_VER" || exit 1

build_one() {
    local target=$1
    local out_dir=$VT_BUILD_ROOT/out-$target
    local host_out_dir=$VT_HOST_OUT_ROOT/$target

    rm -rf "$out_dir" "$host_out_dir"
    mkdir -p "$out_dir"

    case "$target" in
        x86_64-efi)
            echo '======== build grub2.06 for x86_64-efi =============='
            make distclean >/dev/null 2>&1 || true
            ./autogen.sh
            ./configure --disable-werror --with-platform=efi --prefix="$out_dir/" || exit 1
            ;;
        i386-efi)
            echo '======== build grub2.06 for i386-efi ================'
            make distclean >/dev/null 2>&1 || true
            ./autogen.sh
            ./configure --disable-werror --target=i386 --with-platform=efi --prefix="$out_dir/" || exit 1
            ;;
        arm64-efi)
            echo '======== build grub2.06 for arm64-efi ==============='
            prepare_arm64_toolchain
            PATH=$PATH:$VT_ARM64_GCC_DIR/bin
            make distclean >/dev/null 2>&1 || true
            ./autogen.sh
            ./configure --disable-werror --prefix="$out_dir/" \
            --target=aarch64 --with-platform=efi \
            --host=x86_64-linux-gnu \
            HOST_CC=x86_64-linux-gnu-gcc \
            BUILD_CC=gcc \
            TARGET_CC=${VT_ARM64_PREFIX}gcc \
            TARGET_OBJCOPY=${VT_ARM64_PREFIX}objcopy \
            TARGET_STRIP=${VT_ARM64_PREFIX}strip TARGET_NM=${VT_ARM64_PREFIX}nm \
            TARGET_RANLIB=${VT_ARM64_PREFIX}ranlib || exit 1
            ;;
        i386-pc)
            echo '======== build grub2.06 for i386-pc ================='
            make distclean >/dev/null 2>&1 || true
            ./autogen.sh
            ./configure --disable-werror --target=i386 --with-platform=pc --prefix="$out_dir/" || exit 1
            ;;
        mips64el-efi)
            echo '======== build grub2.06 for mips64el-efi ============'
            prepare_mips_toolchain
            PATH=$PATH:$VT_MIPS_TOOLCHAIN_DIR/bin
            make distclean >/dev/null 2>&1 || true
            ./autogen.sh
            ./configure --disable-werror --prefix="$out_dir/" \
            --target=mips64el --with-platform=efi \
            --host=x86_64-linux-gnu \
            HOST_CC=x86_64-linux-gnu-gcc \
            BUILD_CC=gcc \
            TARGET_CC="${VT_MIPS_PREFIX}gcc -mabi=64 -Wno-error=cast-align -Wno-error=misleading-indentation" \
            TARGET_OBJCOPY=${VT_MIPS_PREFIX}objcopy \
            TARGET_STRIP=${VT_MIPS_PREFIX}strip TARGET_NM=${VT_MIPS_PREFIX}nm \
            TARGET_RANLIB=${VT_MIPS_PREFIX}ranlib || exit 1
            ;;
        *)
            echo "Unsupported target: $target"
            exit 1
            ;;
    esac

    make -j 16 || exit 1
    make install || exit 1
    mkdir -p "$host_out_dir"
    tar -C "$out_dir" -cf - . | tar -C "$host_out_dir" -xf -
}

stage_ventoy_layout() {
    rm -rf "$VT_GRUB_DIR/PXE" "$VT_GRUB_DIR/NBP"
    mkdir -p "$VT_GRUB_DIR/PXE" "$VT_GRUB_DIR/NBP"

    VT_GRUB_PREFIX_ROOT="$VT_HOST_OUT_ROOT" \
    VT_TOOL_ROOT="$VT_HOST_OUT_ROOT/x86_64-efi" \
    sh "$VT_GRUB_DIR/install.sh" uefi

    VT_GRUB_PREFIX_ROOT="$VT_HOST_OUT_ROOT" \
    VT_TOOL_ROOT="$VT_HOST_OUT_ROOT/x86_64-efi" \
    sh "$VT_GRUB_DIR/install.sh" i386efi

    VT_GRUB_PREFIX_ROOT="$VT_HOST_OUT_ROOT" \
    VT_TOOL_ROOT="$VT_HOST_OUT_ROOT/x86_64-efi" \
    sh "$VT_GRUB_DIR/install.sh" arm64

    if [ -d "$VT_HOST_OUT_ROOT/mips64el-efi" ]; then
        VT_GRUB_PREFIX_ROOT="$VT_HOST_OUT_ROOT" \
        VT_TOOL_ROOT="$VT_HOST_OUT_ROOT/x86_64-efi" \
        sh "$VT_GRUB_DIR/install.sh" mips64el
    fi

    VT_GRUB_PREFIX_ROOT="$VT_HOST_OUT_ROOT" \
    VT_TOOL_ROOT="$VT_HOST_OUT_ROOT/x86_64-efi" \
    sh "$VT_GRUB_DIR/install.sh"
}

if [ "$VT_TARGET" = "all" ]; then
    build_one x86_64-efi
    build_one i386-efi
    build_one arm64-efi
    if [ "$VT_ENABLE_MIPS64EL" = "1" ]; then
        build_one mips64el-efi
    fi
    build_one i386-pc

    if [ "${VT_STAGE_VENTOY_LAYOUT:-1}" = "1" ]; then
        stage_ventoy_layout
    fi
else
    build_one "$VT_TARGET"
fi

cd "$VT_GRUB_DIR" || exit 1

echo '======== SUCCESS ============='
