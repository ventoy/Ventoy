#!/bin/sh

set -e

TEMPDIR="$(mktemp --directory --tmpdir "vblade.init.generate.$$.XXXXX")"
trap "cd / ; rm -rf \"$TEMPDIR\"" EXIT

run () {
    local OUTPUT="$1"
    echo "I: Processing $OUTPUT"
    TEMP="$TEMPDIR/$OUTPUT"
    shift
    tpage "$@" vblade.init.in>"$TEMP"
    sh -n "$TEMP"
    if [ -f "$OUTPUT" ] && cmp -s "$TEMP" "$OUTPUT" ; then
        echo "I: $OUTPUT is fresh"
    else
        cp "$TEMP" "$OUTPUT"
    fi
}

# run 'vblade.init.debian'        --define lsb=1  --define control=ssd
run 'vblade.init.lsb-daemon'    --define lsb=1  --define control=daemon
run 'vblade.init.daemon'        --define lsb=   --define control=daemon
