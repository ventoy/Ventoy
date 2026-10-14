#!/bin/bash

VENTOY_PATH="$PWD/.."
EPOCH="${SOURCE_DATE_EPOCH:-1700000000}"

rm -f ventoy_unix.cpio

mv ./ventoy_unix/DragonFly ./ 2>/dev/null || true

find ./ventoy_unix -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
find ./ventoy_unix | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > ventoy_unix.cpio
touch -h -d @"$EPOCH" ventoy_unix.cpio

mv ./DragonFly ./ventoy_unix/ 2>/dev/null || true

echo '======== SUCCESS ============='

mkdir -p "$VENTOY_PATH/INSTALL/ventoy"
cp -a ventoy_unix.cpio "$VENTOY_PATH/INSTALL/ventoy/"
