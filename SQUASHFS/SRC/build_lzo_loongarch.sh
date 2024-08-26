#!/bin/bash

LIBDIR=$PWD/../LIB/LZO
LZODIR=lzo-2.08

rm -rf $LIBDIR
rm -rf $LZODIR
tar -xf ${LZODIR}.tar.gz


cd $LZODIR
./configure --prefix=$LIBDIR --disable-shared build_alias=loongarch64-unknown-linux-gnu

make && make install

cd ..
rm -rf $LZODIR

if [ -d $LIBDIR ]; then
    echo -e "\n========== SUCCESS ============\n"
else
    echo -e "\n========== FAILED ============\n"
fi

