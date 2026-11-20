#!/bin/sh
# Download one upstream archive into .cache/upstream and print its path.
# Usage: build/download.sh URL NAME
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
url=$1
name=$2
mkdir -p "$ROOT/.cache/upstream"
dest="$ROOT/.cache/upstream/$name"
if [ ! -s "$dest" ]; then
    wget -O "$dest.part" "$url"
    mv "$dest.part" "$dest"
fi
printf '%s\n' "$dest"
