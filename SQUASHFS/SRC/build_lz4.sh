#!/bin/bash

LIBDIR=$PWD/../LIB/LZ4

rm -rf $LIBDIR
rm -rf lz4-1.10.0
tar -xf lz4-1.10.0.tar.gz


cd lz4-1.10.0
make && PREFIX=$LIBDIR make install

cd ..
rm -rf lz4-1.10.0

if [ -d $LIBDIR ]; then
    echo -e "\n========== SUCCESS ============\n"
else
    echo -e "\n========== FAILED ============\n"
fi


