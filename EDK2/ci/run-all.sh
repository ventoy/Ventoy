#!/bin/sh
# Rebuild every EFI binary Ventoy compiles itself (Ventoy/VtoyUtil apps and the
# iso9660/udf EfiFs drivers, for x64/ia32/aa64) and check that each one matches the
# copy shipped in a Ventoy release byte for byte, apart from Ventoy's Authenticode
# signature (see compare_signed_pe.py).
#
# Usage: sh EDK2/ci/run-all.sh [VERSION]     (default: VENTOY_VERSION in INSTALL/grub/grub.cfg)
# Needs on the host: docker, curl, git, patch, python3, xz, mtools (mcopy), sha256sum.
# Downloads are cached in EDK2/ci/cache and checked against pinned SHA-256s.
set -eu
cd "$(dirname "$0")/../.."
ROOT=$PWD
CI=$ROOT/EDK2/ci
CACHE=$CI/cache
OUT=$CI/out
REF=$CI/ref
VER=${1:-$(sed -n 's/.*VENTOY_VERSION="\([^"]*\)".*/\1/p' INSTALL/grub/grub.cfg | head -1)}
mkdir -p "$CACHE" "$OUT" "$REF"

CENTOS7=centos@sha256:be65f488b7764ad3638f236b7b515b3678369a5124c47b8d32916d6487418ea4
EDK2_ZIP_SHA256=c6f691aa91afbaab811a369fe729f61d8e5b58bb5ad79a45446c9ee849c1a60b
LINARO=gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu
# From ventoy/vtoytoolchain release 1.0; Linaro no longer serves the original download.
LINARO_SHA256=27f1dc2c491ed61ae8f0d4b0c11de59cd2f7dd9c94761ee7153006fcac1bf9ab
EFIFS_URL=https://github.com/pbatard/efifs.git
EFIFS_OLD=6e00f7e   # base of Ventoy's EDK2/efiffs/org; used for the ia32/aa64 drivers
EFIFS_X64=61494b4   # x64 drivers were built in 2020-04 from efifs master (37e58c9..61494b4)

log() { echo "== $*"; }

fetch() {  # url out sha256
    if [ -f "$2" ] && echo "$3  $2" | sha256sum -c --quiet - 2>/dev/null; then return; fi
    curl -fsSL --retry 3 -o "$2" "$1"
    echo "$3  $2" | sha256sum -c --quiet -
}

efifs() {  # commit -> $CACHE/efifs-<commit>, with efifs's GRUB patch and Ventoy's patch
    d=$CACHE/efifs-$1
    if [ ! -d "$d" ]; then
        git clone -q "$EFIFS_URL" "$d"
        git -C "$d" checkout -q "$1"
        git -C "$d" submodule update -q --init grub gnu-efi
    fi
    git -C "$d/grub" checkout -q -- .
    git -C "$d" checkout -q -- src
    (cd "$d/grub" && git apply ../0000-GRUB-fixes.patch)
    (cd "$d" && patch -s -p1 < "$CACHE/ventoy_efifs.patch")
}

build() {  # extra docker args..., then the script and its env
    docker run --rm -v "$ROOT":/ventoy -v "$CACHE":/cache:ro -w /ventoy "$@"
}

log "build image (repo Dockerfile on $CENTOS7)"
docker pull -q "$CENTOS7" >/dev/null
docker tag "$CENTOS7" centos:7
docker build -q -t ventoy-build:ci "$ROOT" >/dev/null

log "inputs"
fetch https://codeload.github.com/tianocore/edk2/zip/edk2-stable201911 \
      EDK2/edk2-edk2-stable201911.zip "$EDK2_ZIP_SHA256"
fetch "https://github.com/ventoy/vtoytoolchain/releases/download/1.0/$LINARO.tar.xz" \
      "$CACHE/$LINARO.tar.xz" "$LINARO_SHA256"
[ -d "$CACHE/$LINARO" ] || tar xf "$CACHE/$LINARO.tar.xz" -C "$CACHE"
(cd EDK2/efiffs && diff -ruN org mod > "$CACHE/ventoy_efifs.patch") || [ $? -eq 1 ]
efifs "$EFIFS_OLD"
efifs "$EFIFS_X64"

AA64PATH="PATH=/usr/sbin:/usr/bin:/sbin:/bin:/cache/$LINARO/bin"
log "build Ventoy/VtoyUtil apps"
build -e "$AA64PATH" -e ARCHS="X64 IA32 AARCH64" -e OUT=/ventoy/EDK2/ci/out \
      ventoy-build:ci sh EDK2/build_repro_all.sh
log "build EfiFs drivers (ia32/aa64 from efifs $EFIFS_OLD)"
build -v "$CACHE/efifs-$EFIFS_OLD":/efifs:ro -e "$AA64PATH" -e ARCHS="IA32 AARCH64" \
      -e PREPATCHED=1 -e OUT=/ventoy/EDK2/ci/out ventoy-build:ci sh EDK2/build_efifs_repro.sh
log "build EfiFs drivers (x64 from efifs $EFIFS_X64)"
build -v "$CACHE/efifs-$EFIFS_X64":/efifs:ro -e ARCHS=X64 \
      -e PREPATCHED=1 -e OUT=/ventoy/EDK2/ci/out ventoy-build:ci sh EDK2/build_efifs_repro.sh

log "reference: ventoy-$VER release"
base=https://github.com/ventoy/Ventoy/releases/download/v$VER
tgz=ventoy-$VER-linux.tar.gz
curl -fsSL --retry 3 -o "$CACHE/sha256-$VER.txt" "$base/sha256.txt"
sum=$(awk -v f="$tgz" '$2==f {print $1}' "$CACHE/sha256-$VER.txt")
[ -n "$sum" ] || { echo "no checksum for $tgz in sha256.txt"; exit 1; }
fetch "$base/$tgz" "$CACHE/$tgz" "$sum"
tar xzf "$CACHE/$tgz" -C "$CACHE" "./ventoy-$VER/ventoy/ventoy.disk.img.xz"
xz -dc "$CACHE/ventoy-$VER/ventoy/ventoy.disk.img.xz" > "$CACHE/ventoy.disk.img"
rm -f "$REF"/*.efi
mcopy -n -i "$CACHE/ventoy.disk.img" '::/ventoy/*.efi' "$REF/"

log "compare"
fail=0
for a in x64 ia32 aa64; do
    for m in ventoy vtoyutil iso9660 udf; do
        f=${m}_$a.efi
        if python3 -I "$CI/compare_signed_pe.py" "$OUT/$f" "$REF/$f"; then :; else fail=1; fi
        if python3 -I "$CI/compare_signed_pe.py" "$OUT/$f" "INSTALL/ventoy/$f" >/dev/null; then
            echo "  committed INSTALL/ventoy/$f: matches"
        else
            echo "  committed INSTALL/ventoy/$f: does NOT match (stale blob?)"
        fi
    done
done
[ "$fail" = 0 ] && log "ALL MATCH ventoy-$VER" || log "MISMATCH against ventoy-$VER"
exit "$fail"
