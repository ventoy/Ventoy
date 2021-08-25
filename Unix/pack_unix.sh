#!/bin/bash

VENTOY_PATH=$PWD/../

rm -f ventoy_unix.cpio

mv ./ventoy_unix/DragonFly ./ 
find ./ventoy_unix | cpio  -o -H newc>ventoy_unix.cpio
mv ./DragonFly ./ventoy_unix/

echo '======== SUCCESS ============='

rm -f $VENTOY_PATH/INSTALL/ventoy/ventoy_unix.cpio
cp -a ventoy_unix.cpio $VENTOY_PATH/INSTALL/ventoy/

