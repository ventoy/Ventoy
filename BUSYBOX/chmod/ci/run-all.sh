#!/bin/sh
# One-shot: fetch toolchains, build the CentOS 7.5 image, run Ventoy's
# BUSYBOX/chmod/build.sh inside it, verify byte-identical, smoke test.
# Needs docker and curl on an x86_64 host. Same steps as the workflow.
set -eu
CI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TOP=$(CDPATH= cd -- "$CI/../../.." && pwd)
step() { printf '\n==== %s ====\n' "$1"; }

step "fetch toolchains"
sh "$CI/fetch.sh"

step "build CentOS 7.5 image"
docker build -t vtchmod-centos75 -f "$CI/Dockerfile.centos75" "$CI"

step "run BUSYBOX/chmod/build.sh in the container"
mkdir -p "$CI/out"
docker run --rm -v "$TOP:/src:ro" -v "$CI/dl:/dl:ro" -v "$CI/out:/out" \
    vtchmod-centos75 bash /src/BUSYBOX/chmod/ci/build.sh
# results are written as root inside the container; hand them back
docker run --rm -v "$CI/out:/out" vtchmod-centos75 chown -R "$(id -u):$(id -g)" /out

step "verify against committed"
sh "$CI/verify.sh"

step "smoke test"
sh "$CI/test.sh"
