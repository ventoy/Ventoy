#!/bin/sh
# Build Ventoy's UEFI applications from a current EDK2 stable tag.
# The applications stay in this repo. This script copies them into a
# source checkout and appends a [Components] section, instead of keeping
# a second copy of edk2-stable201911.
#
# Usage: build/build-edk2.sh [X64|IA32|AARCH64|LOONGARCH64]...

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/build/versions.env"

SRC="$ROOT/.cache/edk2"
APP_SRC="$ROOT/EDK2/edk2_mod/edk2-edk2-stable201911/MdeModulePkg/Application"
OUT="$ROOT/build/out/edk2"

if [ ! -d "$SRC/.git" ]; then
    git clone --depth 1 --branch "$EDK2_TAG" "$EDK2_URL" "$SRC"
fi
git -C "$SRC" submodule update --init --depth 1 \
    MdePkg/Library/MipiSysTLib/mipisyst \
    MdeModulePkg/Library/BrotliCustomDecompressLib/brotli \
    BaseTools/Source/C/BrotliCompress/brotli \
    CryptoPkg/Library/OpensslLib/openssl \
    UnitTestFrameworkPkg/Library/CmockaLib/cmocka \
    MdeModulePkg/Universal/RegularExpressionDxe/oniguruma \
    RedfishPkg/Library/JsonLib/jansson || true

cp -a "$APP_SRC/Ventoy" "$APP_SRC/VtoyUtil" "$APP_SRC/VtoyShim" "$APP_SRC/VDiskChain" \
    "$SRC/MdeModulePkg/Application/"

DSC="$SRC/MdeModulePkg/MdeModulePkg.dsc"
if ! grep -q 'Application/Ventoy/Ventoy.inf' "$DSC"; then
    cat >> "$DSC" << 'EOF'

[Components]
  MdeModulePkg/Application/Ventoy/Ventoy.inf
  MdeModulePkg/Application/VtoyUtil/VtoyUtil.inf
  MdeModulePkg/Application/VtoyShim/VtoyShim.inf
  MdeModulePkg/Application/VDiskChain/VDiskChain.inf
EOF
fi

export PACKAGES_PATH="$SRC"
export EDK_TOOLS_PATH="$SRC/BaseTools"
export WORKSPACE="$SRC"
make -C "$SRC/BaseTools" -j"$(nproc)"
# edksetup.sh reads the caller's positional parameters. Hide the arch list.
setup_edk() {
    set --
    set +u
    cd "$SRC"
    # shellcheck disable=SC1091
    . ./edksetup.sh
    cd "$ROOT"
    set -u
}
requested="$*"
setup_edk

if [ -z "$requested" ]; then
    set -- X64
    command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 && set -- "$@" AARCH64
    command -v loongarch64-linux-gnu-gcc >/dev/null 2>&1 && set -- "$@" LOONGARCH64
else
    # shellcheck disable=SC2086
    set -- $requested
fi

# VtoyShim is installed under the name shim looks up: fbx64.efi, fbia32.efi, fbaa64.efi.
# The 32-byte 0x26 placeholder stays so ventoy_pack.sh can write the GRUB hash into it.
install_vtoy_shim() {
    arch=$1
    case "$arch" in
        X64) name=fbx64.efi ;;
        IA32) name=fbia32.efi ;;
        AARCH64) name=fbaa64.efi ;;
        *) return 0 ;;
    esac
    src="$OUT/$arch/VtoyShim.efi"
    if [ ! -f "$src" ]; then
        return 0
    fi
    case "$arch" in
        AARCH64) oc=aarch64-linux-gnu-objcopy; od=aarch64-linux-gnu-objdump ;;
        *) oc=objcopy; od=objdump ;;
    esac
    sbat="$APP_SRC/VtoyShim/sbat.csv"
    dst="$ROOT/INSTALL/EFI/BOOT/$name"
    mkdir -p "$ROOT/INSTALL/EFI/BOOT"
    "$oc" \
        --add-section ".sbat=$sbat" \
        --set-section-flags .sbat=alloc,load,readonly,data \
        "$src" "$dst"

    tmpfile=$(mktemp)
    "$od" -h "$dst" | grep -E '^[[:space:]]*[0-9]+ \.' > "$tmpfile"
    sbat_size=$(stat -c '%s' "$sbat")
    lmax=0
    lsize=0
    while read -r line; do
        lbase=$(echo "$line" | awk '{print $4}')
        lbase=$((0x$lbase))
        if [ "$lbase" -gt "$lmax" ]; then
            lmax=$lbase
            lsize=$(echo "$line" | awk '{print $3}')
            lsize=$((0x$lsize))
        fi
    done < "$tmpfile"
    rm -f "$tmpfile"
    lvma=$((lsize + lmax))
    lvma_align=$(( (lvma + 4095) / 4096 * 4096 ))
    "$oc" --adjust-section-vma ".sbat=$(printf '0x%08x' "$lvma_align")" "$dst"

    img_base=$("$od" -p "$dst" | awk '/ImageBase/ {print $2; exit}')
    size_img=$("$od" -p "$dst" | awk '/SizeOfImage/ {print $2; exit}')
    sbat_end=$((lvma_align + sbat_size))
    # PE32 objdump prints eight hex digits. PE32+ prints sixteen. A relocatable
    # EFI image has image base zero in either form.
    if [ -z "$img_base" ] || [ "$((0x$img_base))" -ne 0 ]; then
        echo "ImageBase of $name is ${img_base:-missing}" >&2
        exit 1
    fi
    if [ "$((0x$size_img))" -lt "$sbat_end" ]; then
        echo "SizeOfImage 0x$size_img does not cover the SBAT section" >&2
        exit 1
    fi
    magic=$(hexdump -C "$dst" | grep -c '26 26 26 26 26 26 26 26' || true)
    if [ "$magic" -ne 1 ]; then
        echo "$name hash placeholder count is $magic, expected 1" >&2
        exit 1
    fi
    echo "installed $dst"
}

mkdir -p "$OUT"
for arch in "$@"; do
    # GCC_BIN is the prefix for both X64 and IA32. Clear it on every arch so
    # an IA32 build does not leave i686-linux-gnu- in place for X64.
    export GCC_BIN=
    case "$arch" in
        X64) ;;
        IA32) export GCC_BIN=i686-linux-gnu- ;;
        AARCH64) export GCC_AARCH64_PREFIX=aarch64-linux-gnu- ;;
        LOONGARCH64) export GCC_LOONGARCH64_PREFIX=loongarch64-linux-gnu- ;;
        *) echo "unknown arch $arch" >&2; exit 1 ;;
    esac
    # Current EDK2 accepts only one -m per invocation.
    # fbx64 follows the x64 Rocky shim layout. LoongArch boots the GRUB image directly.
    modules="Ventoy VtoyUtil"
    if [ "$arch" != "LOONGARCH64" ]; then
        modules="$modules VtoyShim VDiskChain"
    fi
    for mod in $modules; do
        build -p MdeModulePkg/MdeModulePkg.dsc -a "$arch" -b RELEASE -t GCC \
            -m "MdeModulePkg/Application/$mod/$mod.inf"
    done
    mkdir -p "$OUT/$arch"
    find "$SRC/Build/MdeModule/RELEASE_GCC/$arch" -name '*.efi' -path '*/OUTPUT/*.efi' \
        -exec cp -a {} "$OUT/$arch/" \;
    install_vtoy_shim "$arch"
done

find "$OUT" -name '*.efi' -printf '%p %s\n' | sort
