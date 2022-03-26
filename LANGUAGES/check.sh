#!/bin/sh

VTOY_PATH=$1

if [ ! -f $VTOY_PATH/LANGUAGES/languages.json ]; then
    exit 1
fi

gcc -DFOR_VTOY_JSON_CHECK $VTOY_PATH/Ventoy2Disk/Ventoy2Disk/VentoyJson.c -I $VTOY_PATH/Ventoy2Disk/Ventoy2Disk/ -o checkjson

./checkjson $VTOY_PATH/LANGUAGES/languages.json
ret=$?

rm -f ./checkjson
[ $ret -eq 0 ]


