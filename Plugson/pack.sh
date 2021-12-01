#!/bin/sh

output_hex_u32() {
    hexval=$(printf '%08x' $1)
    hex_B0=${hexval:0:2}
    hex_B1=${hexval:2:2}
    hex_B2=${hexval:4:2}
    hex_B3=${hexval:6:2}
    echo -en "\x$hex_B3\x$hex_B2\x$hex_B1\x$hex_B0"
}

if [ -n "$PKG_DATE" ]; then
    plugson_verion=$PKG_DATE
else
    plugson_verion=$(date '+%Y%m%d %H:%M:%S')
fi

sed "s#.*plugson_build_date.*#                <b id=\"plugson_build_date\">$plugson_verion</b>#" -i ./www/index.html

if [ ! -f ./vs/VentoyPlugson/Release/VentoyPlugson.exe ]; then
    echo "NO VentoyPlugson.exe found"
    exit 1
fi

if [ -f ./www.tar.xz ]; then
    rm -f ./www.tar.xz
fi

echo -n "$plugson_verion" > ./www/buildtime

tar cf www.tar www
xz --check=crc32 www.tar

xzdec=$(stat -c '%s' ./www.tar.xz)
echo xzdec=$xzdec

output_hex_u32 0x54535251    > ex.bin
output_hex_u32 $xzdec       >> ex.bin
output_hex_u32 0xa4a3a2a1   >> ex.bin

cat ./vs/VentoyPlugson/Release/VentoyPlugson.exe ./www.tar.xz ex.bin > VentoyPlugson.exe
rm -f ./ex.bin

rm -f ../INSTALL/VentoyPlugson.exe
cp -a ./VentoyPlugson.exe ../INSTALL/VentoyPlugson.exe

rm -f ../INSTALL/tool/plugson.tar.xz
mv ./www.tar.xz ../INSTALL/tool/plugson.tar.xz

echo ""
echo "========= SUCCESS ==========="
echo ""


