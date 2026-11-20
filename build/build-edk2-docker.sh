#!/bin/sh
# Build Ventoy's UEFI applications inside Debian trixie.
# Usage: build/build-edk2-docker.sh [X64|IA32|AARCH64|LOONGARCH64]...

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
image=${VENTOY_USER_IMAGE:-ventoy-trixie}
arches=$(printf '%s ' "$@")

docker run --rm \
    -v "$ROOT:/ventoy" \
    -w /ventoy \
    -e HOME=/tmp \
    "$image" \
    sh -c "sh build/build-edk2.sh $arches; status=\$?; chown -R $(id -u):$(id -g) build/out/edk2 .cache/edk2/BaseTools .cache/edk2/Build .cache/edk2/Conf || true; exit \$status"
