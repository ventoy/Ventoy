#!/bin/sh
# Byte-compare everything build.sh produced (ci/out/<path>) against the
# committed file at the same <path>: the five BUSYBOX/chmod/vtchmod* and the
# five copies build.sh installs under IMG/cpio_*/ventoy/busybox
# (BLOB_List.md: "Same with ./BUSYBOX/chmod/<name>, check the file hash").
set -eu
CI=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TOP=$(CDPATH= cd -- "$CI/../../.." && pwd)
OUT=$CI/out
fail=0; n=0
printf '%-8s %-64s %s\n' result sha256 path
for f in $(cd "$OUT" && find BUSYBOX IMG -type f 2>/dev/null | sort); do
    n=$((n+1))
    h=$(sha256sum "$OUT/$f" | cut -d' ' -f1)
    if [ ! -f "$TOP/$f" ]; then
        printf '%-8s %-64s %s\n' MISSING "$h" "$f"; fail=1
    elif cmp -s "$TOP/$f" "$OUT/$f"; then
        printf '%-8s %-64s %s\n' MATCH "$h" "$f"
    else
        printf '%-8s %-64s %s (committed %s)\n' DIFF "$h" "$f" "$(sha256sum "$TOP/$f" | cut -c1-12)"; fail=1
    fi
done
[ "$n" -eq 10 ] || { echo "verify: expected 10 files, found $n"; fail=1; }
[ "$fail" -eq 0 ] && echo "verify: all identical" || echo "verify: FAILED"
exit "$fail"
