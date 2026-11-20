#!/bin/sh
# Build the USB tools inside the Debian trixie image.
# Usage: build/build-user-docker.sh [x86_64|aarch64|loongarch64|mips64el|i386]...
# With no arguments, build every architecture.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
image=${VENTOY_USER_IMAGE:-ventoy-trixie}

if [ "$#" -eq 0 ]; then
    set -- x86_64 aarch64 loongarch64 mips64el i386
fi

arches=$(printf '%s ' "$@")
chown_paths=
for arch in "$@"; do
    chown_paths="$chown_paths build/out/tools/$arch build/work/user/$arch"
done

docker run --rm \
    -v "$ROOT:/ventoy" \
    -w /ventoy \
    -e HOME=/tmp \
    -e "VENTOY_ONLY=${VENTOY_ONLY:-}" \
    "$image" \
    sh -c "sh build/build-user.sh $arches; status=\$?; chown -R $(id -u):$(id -g) $chown_paths || true; exit \$status"
