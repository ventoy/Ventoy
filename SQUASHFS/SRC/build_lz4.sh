#!/bin/bash

LIBDIR=$PWD/../LIB/LZ4

rm -rf $LIBDIR
rm -rf lz4-1.8.1.2
tar -xf lz4-1.8.1.2.tar.gz


cd lz4-1.8.1.2
make && PREFIX=$LIBDIR make install

cd ..
rm -rf lz4-1.8.1.2

if [ -d $LIBDIR ]; then
    echo -e "\n========== SUCCESS ============\n"
else
    echo -e "\n========== FAILED ============\n"
fi


