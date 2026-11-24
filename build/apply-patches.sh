#!/bin/sh
# Apply build/patches/<project>/series onto an extracted upstream tree.
# Usage: build/apply-patches.sh <project> <destination>

set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <project> <destination>" >&2
    exit 1
fi

ROOT=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
SERIES="$ROOT/patches/$1/series"
DEST=$2

if [ ! -f "$SERIES" ]; then
    echo "no patch series $SERIES" >&2
    exit 1
fi
if [ ! -d "$DEST" ]; then
    echo "destination does not exist: $DEST" >&2
    exit 1
fi

while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        ""|"#"*) continue ;;
    esac
    patch=${line%%#*}
    patch=$(printf '%s' "$patch" | sed 's/[[:space:]]*$//')
    [ -n "$patch" ] || continue
    patch -d "$DEST" -p1 --forward --batch < "$ROOT/patches/$1/$patch"
done < "$SERIES"

EMPTY="$ROOT/patches/$1/new-empty"
if [ -f "$EMPTY" ]; then
    while IFS= read -r path || [ -n "$path" ]; do
        case "$path" in
            ""|"#"*) continue ;;
        esac
        mkdir -p "$DEST/$(dirname "$path")"
        : > "$DEST/$path"
    done < "$EMPTY"
fi
