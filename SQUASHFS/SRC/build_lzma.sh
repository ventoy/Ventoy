#!/bin/bash

LIBDIR=$PWD/../LIB/LZMA

rm -rf $LIBDIR
rm -rf liblzma-master
unzip liblzma-master.zip

cd liblzma-master
./configure --prefix=$LIBDIR --disable-xz --disable-xzdec --disable-lzmadec --disable-lzmainfo --enable-small
make -j 8 && make install

cd ..
rm -rf liblzma-master

if [ -d $LIBDIR ]; then
    echo -e "\n========== SUCCESS ============\n"
else
    echo -e "\n========== FAILED ============\n"
fi


