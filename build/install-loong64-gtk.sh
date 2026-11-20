#!/bin/sh
# Download loong64 GTK 3 into a sysroot from the unofficial Debian Trixie
# archive at https://loong13.debian.net/. Official trixie has the
# loongarch64 gcc, but not this architecture's GTK packages.
#
# The packages are not installed with dpkg --add-architecture. That archive
# lags Debian security by a rebuild, so a multiarch install conflicts with
# the newer amd64 libraries already in the image.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
SRC_KEY="$ROOT/build/keys/debian-loong64-non-official-archive-keyring.pgp"
SYSROOT="$ROOT/build/work/loong64-sysroot"
APTROOT="$ROOT/build/work/loong64-apt"
KEY=/usr/share/keyrings/debian-loong64-non-official-archive-keyring.pgp

# libc.so from this archive is a linker script with absolute /lib paths.
# Leave glibc to the cross compiler, which has its own copy.
hide_glibc_linkers() {
    lib="$SYSROOT/usr/lib/loongarch64-linux-gnu"
    rm -f "$lib/libc.so" "$lib/libc.a" "$lib/libc_nonshared.a" \
        "$lib/libm.so" "$lib/libm.a" \
        "$lib/libresolv.so" "$lib/libresolv.a" \
        "$lib/libpthread.a" "$lib/libdl.a" "$lib/librt.a"
}

if [ -f "$SYSROOT/usr/lib/loongarch64-linux-gnu/pkgconfig/gtk+-3.0.pc" ]; then
    hide_glibc_linkers
    exit 0
fi

if [ "$(id -u)" -ne 0 ]; then
    echo "install-loong64-gtk.sh must run as root inside the trixie image" >&2
    exit 1
fi
test -s "$SRC_KEY"
cp -a "$SRC_KEY" "$KEY"

rm -rf "$APTROOT"
mkdir -p "$APTROOT/etc/apt" "$APTROOT/var/lib/apt/lists/partial" \
    "$APTROOT/var/cache/apt/archives/partial" "$APTROOT/var/lib/dpkg"
touch "$APTROOT/var/lib/dpkg/status"
cat > "$APTROOT/etc/apt/sources.list" <<EOF
deb [arch=loong64 signed-by=$KEY] http://loong13.debian.net/debian-loong64 trixie main
EOF
cat > "$APTROOT/etc/apt/apt.conf" <<EOF
APT::Architecture "loong64";
APT::Architectures "loong64";
APT::Install-Recommends "false";
APT::Get::Assume-Yes "true";
Dir "$APTROOT";
Dir::State::status "$APTROOT/var/lib/dpkg/status";
EOF

apt-get -c "$APTROOT/etc/apt/apt.conf" update
apt-get -c "$APTROOT/etc/apt/apt.conf" install -y -d --no-install-recommends libgtk-3-dev

rm -rf "$SYSROOT"
mkdir -p "$SYSROOT"
for deb in "$APTROOT"/var/cache/apt/archives/*.deb; do
    dpkg-deb -x "$deb" "$SYSROOT"
done
hide_glibc_linkers
test -f "$SYSROOT/usr/lib/loongarch64-linux-gnu/pkgconfig/gtk+-3.0.pc"
