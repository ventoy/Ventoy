#!/bin/bash

VENTOY_PATH="$PWD/.."
EPOCH="${SOURCE_DATE_EPOCH:-1700000000}"

rm -f vtloopex.cpio
cp -a vtloopex vtloopex_tmp
cd vtloopex_tmp

for dir in *; do
    if [ -d "$dir" ]; then
        cd "$dir"
        find vtloopex -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
        tar --mtime=@"$EPOCH" --sort=name --owner=0 --group=0 --numeric-owner -cJf vtloopex.tar.xz vtloopex
        rm -rf vtloopex
        cd ..
    fi
done

find . -exec touch -h -d @"$EPOCH" {} + 2>/dev/null || true
find . | LC_ALL=C sort | cpio -o -H newc --reproducible --owner=0:0 > ../vtloopex.cpio
touch -h -d @"$EPOCH" ../vtloopex.cpio

cd ..
rm -rf vtloopex_tmp

mkdir -p "$VENTOY_PATH/INSTALL/ventoy"
cp -a vtloopex.cpio "$VENTOY_PATH/INSTALL/ventoy/"

echo '======== SUCCESS ============='
