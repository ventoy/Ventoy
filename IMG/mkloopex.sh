#!/bin/bash

VENTOY_PATH=$PWD/../

rm -f vtloopex.cpio
cp -a vtloopex vtloopex_tmp
cd vtloopex_tmp


for dir in $(ls); do
    cd $dir    
    tar -cJf vtloopex.tar.xz vtloopex    
    rm -rf vtloopex
    cd ..
done

find . | cpio -o -H newc --owner=root:root >../vtloopex.cpio

cd ..

rm -rf vtloopex_tmp

rm -f $VENTOY_PATH/INSTALL/ventoy/vtloopex.cpio
cp -a vtloopex.cpio $VENTOY_PATH/INSTALL/ventoy/

echo '======== SUCCESS ============='

