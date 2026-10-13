#!/bin/sh
# Smoke tests for ci/out/BUSYBOX/chmod/vtchmod*. Two behaviours matter to
# Ventoy's early init: `vtchmod FILE` must make FILE mode 777, and
# `vtchmod -6` must exit 0 only when uname -m is x86_64. The x86 binaries
# run natively (this host must be x86_64); aarch64/mips64el run through
# qemu-user-static when it is installed and are skipped otherwise.
set -eu
CI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=$CI/out/BUSYBOX/chmod
[ "$(uname -m)" = x86_64 ] || { echo "test: host is $(uname -m), the native x86 checks need x86_64"; exit 1; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0; ran=0
ok()  { printf 'ok    %s\n' "$1"; }
bad() { printf 'FAIL  %s\n' "$1"; fail=1; }

# name  file(1) pattern  qemu binary ("" = native)  expected -6 exit
run_one() {
    n=$1; pat=$2; q=$3; want6=$4
    [ -f "$OUT/$n" ] || { bad "$n missing"; return 0; }
    case "$(file -b "$OUT/$n")" in
        *"$pat"*"statically linked"*) ok "$n is static $pat" ;;
        *) bad "$n file(1): $(file -b "$OUT/$n")" ;;
    esac
    if [ -n "$q" ] && ! command -v "$q" >/dev/null 2>&1; then
        echo "skip  $n: $q not installed"; return 0
    fi
    ran=$((ran+1))
    : > "$tmp/$n"; chmod 000 "$tmp/$n"
    $q "$OUT/$n" "$tmp/$n" || true
    m=$(stat -c %a "$tmp/$n")
    [ "$m" = 777 ] && ok "$n chmod -> 777" || bad "$n chmod -> $m"
    set +e; $q "$OUT/$n" -6; rc=$?; set -e
    [ "$rc" = "$want6" ] && ok "$n -6 exit $rc" || bad "$n -6 exit $rc, wanted $want6"
}

run_one vtchmod64       "x86-64"        ""                      0
run_one vtchmod64_musl  "x86-64"        ""                      0
run_one vtchmod32       "Intel 80386"   ""                      0
# under qemu-user, uname -m still reports the emulated arch, so -6 must fail
run_one vtchmodaa64     "ARM aarch64"   qemu-aarch64-static     1
run_one vtchmodm64e     "MIPS"          qemu-mips64el-static    1

[ "$ran" -gt 0 ] || bad "nothing was executed"
[ "$fail" -eq 0 ] && echo "test: passed" || { echo "test: FAILED"; exit 1; }
