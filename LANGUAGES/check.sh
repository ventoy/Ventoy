#!/bin/sh

VTOY_PATH=$1

if [ ! -f $VTOY_PATH/LANGUAGES/languages.json ]; then
    exit 1
fi

gcc -DFOR_VTOY_JSON_CHECK $VTOY_PATH/Ventoy2Disk/Ventoy2Disk/VentoyJson.c -I $VTOY_PATH/Ventoy2Disk/Ventoy2Disk/ -o checkjson

RET=0

./checkjson $VTOY_PATH/LANGUAGES/languages.json
ret=$?
if [ $ret -eq 0 ]; then
    for i in $(ls $VTOY_PATH/INSTALL/grub/menu); do
        echo "check INSTALL/grub/menu/$i ..."
        ./checkjson $VTOY_PATH/INSTALL/grub/menu/$i
        ret=$?
        if [ $ret -ne 0 ]; then
            echo "INSTALL/grub/menu/$i invalid json format"
            break
        fi
    done
else
    echo "languages.json invalid json format"
fi

rm -f ./checkjson
[ $ret -eq 0 ]

