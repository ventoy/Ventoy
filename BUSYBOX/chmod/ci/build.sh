#!/bin/bash
# Runs INSIDE the CentOS 7.5 container (see Dockerfile.centos75).
# Lays out the toolchains exactly as DOC/BuildVentoyFromSource.txt describes
# (/opt/diet32, /opt/diet64, /usr/local/musl, /opt/<cross toolchain>/bin on
# PATH), then runs Ventoy's own BUSYBOX/chmod/build.sh, unmodified, in a
# writable copy of the tree. Nothing here retypes a compile line.
#   /src : Ventoy checkout (read-only)   /dl : tarballs   /out : results
set -euo pipefail
SRC=/src; DL=/dl; OUT=/out

echo "=== compiler ==="
rpm -q gcc

echo "=== dietlibc 0.34 via DOC/installdietlibc.sh ==="
t=$(mktemp -d); cp "$DL/dietlibc-0.34.tar.xz" "$t/"
( cd "$t" && bash "$SRC/DOC/installdietlibc.sh" >/dev/null 2>&1 )
rm -rf "$t"
ls /opt/diet32/bin/diet /opt/diet64/bin/diet >/dev/null

echo "=== musl 1.2.1 -> /usr/local/musl ==="
t=$(mktemp -d); tar -C "$t" -xf "$DL/musl-1.2.1.tar.gz"
( cd "$t"/musl-1.2.1 && ./configure --prefix=/usr/local/musl >/dev/null && make -j"$(nproc)" >/dev/null && make install >/dev/null )
rm -rf "$t"

echo "=== cross toolchains -> /opt ==="
tar -C /opt -xf "$DL/aarch64--uclibc--stable-2020.08-1.tar.bz2"
tar -C /opt -xf "$DL/mips64el-linux-musl-gcc730.tar.bz2"
mv /opt/output /opt/mips64el-linux-musl-gcc730
export PATH=$PATH:/opt/aarch64--uclibc--stable-2020.08-1/bin:/opt/mips64el-linux-musl-gcc730/bin
aarch64-linux-gcc --version | head -1
mips64el-linux-musl-gcc --version | head -1

echo "=== BUSYBOX/chmod/build.sh (unmodified) ==="
W=$(mktemp -d)
mkdir -p "$W/BUSYBOX" "$W/IMG/cpio_x86/ventoy/busybox" "$W/IMG/cpio_arm64/ventoy/busybox" "$W/IMG/cpio_mips64/ventoy/busybox"
cp -a "$SRC/BUSYBOX/chmod" "$W/BUSYBOX/"
rm -rf "$W/BUSYBOX/chmod/ci"
( cd "$W/BUSYBOX/chmod" && sh build.sh )

echo "=== collect ==="
rm -rf "$OUT/BUSYBOX" "$OUT/IMG"
mkdir -p "$OUT/BUSYBOX/chmod" "$OUT/IMG/cpio_x86/ventoy/busybox" "$OUT/IMG/cpio_arm64/ventoy/busybox" "$OUT/IMG/cpio_mips64/ventoy/busybox"
cp -a "$W"/BUSYBOX/chmod/vtchmod32 "$W"/BUSYBOX/chmod/vtchmod64 "$W"/BUSYBOX/chmod/vtchmod64_musl \
      "$W"/BUSYBOX/chmod/vtchmodaa64 "$W"/BUSYBOX/chmod/vtchmodm64e "$OUT/BUSYBOX/chmod/"
cp -a "$W"/IMG/cpio_x86/ventoy/busybox/vtchmod* "$OUT/IMG/cpio_x86/ventoy/busybox/"
cp -a "$W"/IMG/cpio_arm64/ventoy/busybox/vtchmodaa64 "$OUT/IMG/cpio_arm64/ventoy/busybox/"
cp -a "$W"/IMG/cpio_mips64/ventoy/busybox/vtchmodm64e "$OUT/IMG/cpio_mips64/ventoy/busybox/"
rm -rf "$W"
for n in vtchmod32 vtchmod64 vtchmod64_musl vtchmodaa64 vtchmodm64e; do
    printf '%-16s ' "$n"; readelf -p .comment "$OUT/BUSYBOX/chmod/$n" | sed -n 's/^ *\[ *0\] *//p'
done
