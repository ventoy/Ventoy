#!/bin/sh
# Reproducible build of Ventoy's own EDK2 UEFI apps (Ventoy, VtoyUtil) for X64 and IA32.
# The EDK2 tree must sit at the ORIGINAL path embedded in the shipped binaries
# (/home/share/Ventoy/EDKII/...) so the __FILE__ strings match byte for byte.
# Run inside the repo's CentOS 7 build image with the Ventoy tree mounted at /ventoy.
# Optional: ARCHS="X64 IA32 AARCH64" (AARCH64 needs aarch64-linux-gnu-gcc on PATH).
set -e
ORIG=/home/share/Ventoy/EDKII
OUT=${OUT:-/ventoy/EDK2/repro_out}
ARCHS=${ARCHS:-"X64 IA32"}

rm -rf "$ORIG"; mkdir -p "$ORIG" "$OUT"; cd "$ORIG"
unzip -q /ventoy/EDK2/edk2-edk2-stable201911.zip
cp -a /ventoy/EDK2/edk2_mod/edk2-edk2-stable201911/. ./edk2-edk2-stable201911/
cd edk2-edk2-stable201911
make -j"$(nproc)" -C BaseTools/ >/tmp/basetools.log 2>&1
rm -rf ./Conf/.cache ./Conf/.AutoGenIdFile.txt
unset WORKSPACE; . ./edksetup.sh >/dev/null

for A in $ARCHS; do
    case $A in X64) p=x64;; IA32) p=ia32;; AARCH64) p=aa64;; esac
    for M in Ventoy VtoyUtil; do
        GCC48_AARCH64_PREFIX=aarch64-linux-gnu- \
        build -p MdeModulePkg/MdeModulePkg.dsc -a "$A" -b RELEASE -t GCC48 \
              -m "MdeModulePkg/Application/$M/$M.inf" >"/tmp/build_${M}_$A.log" 2>&1 \
            || { echo "BUILD_FAIL $M $A"; tail -5 "/tmp/build_${M}_$A.log"; continue; }
        lc=$(echo "$M" | tr 'A-Z' 'a-z')
        cp -f "Build/MdeModule/RELEASE_GCC48/$A/MdeModulePkg/Application/$M/$M/OUTPUT/$M.efi" \
              "$OUT/${lc}_${p}.efi"
        echo "BUILD_OK ${lc}_${p}.efi"
    done
done
