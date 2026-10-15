#!/bin/sh
# Reproducible build of Ventoy's iso9660/udf EfiFs drivers.
# Base: pbatard/efifs commit 6e00f7e (GRUB submodule 4dd4ceec0 + efifs 0000-GRUB-fixes.patch),
# mounted read-only at /efifs, plus Ventoy's three modified files from EDK2/efiffs/mod.
# Built as EfiFsPkg inside the EDK2 tree at the original path embedded in the shipped drivers.
# Optional: ARCHS="X64 IA32 AARCH64" (AARCH64 needs aarch64-linux-gnu-gcc on PATH).
set -e
ORIG=/home/share/Ventoy/EDKII
EDK=$ORIG/edk2-edk2-stable201911
OUT=${OUT:-/ventoy/EDK2/repro_out}
ARCHS=${ARCHS:-"X64 IA32"}

rm -rf "$ORIG"; mkdir -p "$ORIG" "$OUT"; cd "$ORIG"
unzip -q /ventoy/EDK2/edk2-edk2-stable201911.zip
cp -a /ventoy/EDK2/edk2_mod/edk2-edk2-stable201911/. "$EDK"/
cp -a /efifs "$EDK/EfiFsPkg"
# PREPATCHED=1: /efifs already has Ventoy's changes applied as a patch (EDK2/ventoy_efifs.patch)
[ "${PREPATCHED:-0}" = 1 ] || cp -a /ventoy/EDK2/efiffs/mod/. "$EDK/EfiFsPkg/"
cd "$EDK"
make -j"$(nproc)" -C BaseTools/ >/tmp/basetools.log 2>&1
rm -rf ./Conf/.cache ./Conf/.AutoGenIdFile.txt
unset WORKSPACE; . ./edksetup.sh >/dev/null

for A in $ARCHS; do
    case $A in X64) p=x64;; IA32) p=ia32;; AARCH64) p=aa64;; esac
    (cd EfiFsPkg && sh ./set_grub_cpu.sh "$A" >/dev/null)
    for M in Iso9660 Udf; do
        GCC48_AARCH64_PREFIX=aarch64-linux-gnu- \
        build -p EfiFsPkg/EfiFsPkg.dsc -a "$A" -b RELEASE -t GCC48 \
              -m "EfiFsPkg/EfiFsPkg/$M.inf" >"/tmp/build_${M}_$A.log" 2>&1 \
            || { echo "BUILD_FAIL $M $A"; tail -8 "/tmp/build_${M}_$A.log"; continue; }
        lc=$(echo "$M" | tr 'A-Z' 'a-z')
        cp -f "$(ls Build/EfiFs/RELEASE_GCC48/$A/EfiFsPkg/EfiFsPkg/$M/OUTPUT/*.efi | head -1)" \
              "$OUT/${lc}_${p}.efi"
        echo "BUILD_OK ${lc}_${p}.efi"
    done
done
