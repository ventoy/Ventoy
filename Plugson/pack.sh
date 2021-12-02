#!/bin/sh

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

rm -f ../INSTALL/VentoyPlugson.exe
cp -a ./vs/VentoyPlugson/Release/VentoyPlugson.exe ../INSTALL/VentoyPlugson.exe

rm -f ../INSTALL/tool/plugson.tar.xz
mv ./www.tar.xz ../INSTALL/tool/plugson.tar.xz

echo ""
echo "========= SUCCESS ==========="
echo ""


