#!/bin/bash

VENTOY_PATH=$PWD/../

rm -f ventoy_unix.cpio

find ./ventoy_unix | cpio  -o -H newc>ventoy_unix.cpio

echo '======== SUCCESS ============='

rm -f $VENTOY_PATH/INSTALL/ventoy/ventoy_unix.cpio
cp -a ventoy_unix.cpio $VENTOY_PATH/INSTALL/ventoy/

