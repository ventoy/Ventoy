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

if [ ! -f ./vs/VentoyPlugson/x64/Release/VentoyPlugson_X64.exe ]; then
    echo "NO VentoyPlugson_X64.exe found"
    exit 1
fi

if [ -f ./www.tar.xz ]; then
    rm -f ./www.tar.xz
fi

VV=$(grep -m1 '\?v=' ./www/index.html |   sed  's/.*v=\([0-9][0-9]*\).*/\1/g')
let VV++
echo V=$VV
sed "s/\?v=[0-9][0-9]*/?v=$VV/g" -i ./www/index.html 


[ -f ./www/helplist ] && rm -f ./www/helplist
ls -1 ../INSTALL/grub/help/ | while read line; do 
    echo -n ${line:0:5} >> ./www/helplist
done 
[ -f ./www/menulist ] && rm -f ./www/menulist
ls -1 ../INSTALL/grub/menu/ | while read line; do 
    echo -n ${line:0:5} >> ./www/menulist
done 
echo -n "$plugson_verion" > ./www/buildtime

tar cf www.tar www
xz --check=crc32 www.tar

rm -f ../INSTALL/VentoyPlugson.exe
cp -a ./vs/VentoyPlugson/Release/VentoyPlugson.exe ../INSTALL/VentoyPlugson.exe

rm -f ../INSTALL/VentoyPlugson_X64.exe
cp -a ./vs/VentoyPlugson/x64/Release/VentoyPlugson_X64.exe ../INSTALL/VentoyPlugson_X64.exe


rm -f ../INSTALL/tool/plugson.tar.xz
mv ./www.tar.xz ../INSTALL/tool/plugson.tar.xz

echo ""
echo "========= SUCCESS ==========="
echo ""


