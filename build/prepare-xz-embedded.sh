#!/bin/sh
# Download xz-embedded and print the extracted directory.
# A stable symlink .cache/upstream/xz-embedded points at the pinned version.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$ROOT/build/versions.env"
archive=$("$ROOT/build/download.sh" "$XZ_EMBEDDED_URL" "xz-embedded-${XZ_EMBEDDED_VERSION}.tar.gz")
dest="$ROOT/.cache/upstream/xz-embedded-${XZ_EMBEDDED_VERSION}"
link="$ROOT/.cache/upstream/xz-embedded"
if [ ! -f "$dest/linux/lib/decompress_unxz.c" ]; then
    rm -rf "$dest"
    mkdir -p "$dest"
    tar -xf "$archive" -C "$dest" --strip-components=1
fi
if grep -q '^STATIC int INIT unxz' "$dest/linux/lib/decompress_unxz.c"; then
    patch -p1 -d "$dest" < "$ROOT/build/patches/xz-embedded/0001-export-unxz.patch" >/dev/null
fi
ln -sfn "xz-embedded-${XZ_EMBEDDED_VERSION}" "$link"
printf '%s\n' "$dest"
