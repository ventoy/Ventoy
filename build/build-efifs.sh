#!/bin/sh
# Build iso9660 and udf UEFI drivers from EfiFs, with Ventoy's filename
# and buffer-size behaviour. Run this inside the Debian trixie image.
#
# Usage: build/build-efifs.sh [X64|IA32|AARCH64|LOONGARCH64]...

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/build/versions.env"

EFIFS="$ROOT/.cache/efifs"
EDK="$ROOT/.cache/edk2"
OUT="$ROOT/build/out/efifs"

if [ ! -d "$EFIFS/.git" ]; then
    git clone --depth 1 --branch "$EFIFS_TAG" "$EFIFS_URL" "$EFIFS"
fi
if [ ! -d "$EFIFS/grub/grub-core" ]; then
    git -C "$EFIFS" submodule update --init --depth 1 grub
fi
if ! grep -q g_fs_name_nocase "$EFIFS/src/logging.c"; then
    patch -p1 -d "$EFIFS" < "$ROOT/build/patches/efifs/0001-ventoy-filesystem-drivers.patch"
fi
if ! grep -q '#ifndef ARRAY_SIZE' "$EFIFS/grub/include/grub/misc.h"; then
    patch -p1 -d "$EFIFS/grub" < "$EFIFS/0001-GRUB-fixes.patch"
fi

if [ ! -d "$EDK/.git" ]; then
    git clone --depth 1 --branch "$EDK2_TAG" "$EDK2_URL" "$EDK"
fi
ln -sfn "$EFIFS" "$EDK/EfiFsPkg"

if ! command -v python >/dev/null 2>&1; then
    ln -sf "$(command -v python3)" /usr/local/bin/python
fi

stamp=$(gcc -dumpversion)-$(gcc -dumpmachine)
if [ "$(cat "$EDK/BaseTools/.ventoy-compiler" 2>/dev/null || true)" != "$stamp" ]; then
    make -C "$EDK/BaseTools" clean
    make -C "$EDK/BaseTools" -j"$(nproc)"
    echo "$stamp" > "$EDK/BaseTools/.ventoy-compiler"
fi

export PACKAGES_PATH="$EDK"
export EDK_TOOLS_PATH="$EDK/BaseTools"
export WORKSPACE="$EDK"
export GCC_AARCH64_PREFIX="${GCC_AARCH64_PREFIX:-aarch64-linux-gnu-}"
export GCC_LOONGARCH64_PREFIX="${GCC_LOONGARCH64_PREFIX:-loongarch64-linux-gnu-}"

setup_edk() {
    set --
    set +u
    cd "$EDK"
    # shellcheck disable=SC1091
    . ./edksetup.sh
    cd "$ROOT"
    set -u
}
requested="$*"
setup_edk
if [ -z "$requested" ]; then
    set -- X64 IA32 AARCH64 LOONGARCH64
else
    # shellcheck disable=SC2086
    set -- $requested
fi

mkdir -p "$OUT"
export GCC_BIN=
for arch in "$@"; do
    case "$arch" in
        X64) short=x64; GCC_BIN= ;;
        IA32) short=ia32; GCC_BIN=i686-linux-gnu- ;;
        AARCH64) short=aa64; GCC_BIN= ;;
        LOONGARCH64) short=loongarch64; GCC_BIN= ;;
        *) echo "unknown arch $arch" >&2; exit 1 ;;
    esac
    export GCC_BIN
    bash "$EFIFS/set_grub_cpu.sh" "$arch"
    for mod in Iso9660 Udf; do
        build -p EfiFsPkg/EfiFsPkg.dsc -a "$arch" -b RELEASE -t GCC \
            -m "EfiFsPkg/EfiFsPkg/$mod.inf"
        base=$(printf '%s' "$mod" | tr '[:upper:]' '[:lower:]')
        found=$(find "$EDK/Build/EfiFs" -path "*${arch}/${base}.efi" | head -n 1)
        if [ -z "$found" ]; then
            echo "missing ${base}.efi for $arch" >&2
            exit 1
        fi
        cp -a "$found" "$OUT/${base}_${short}.efi"
    done
done

ls -l "$OUT"
