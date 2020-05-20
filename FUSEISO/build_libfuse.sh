#!/bin/bash

#
#
# Package Dependency:
# gcc automake autoconf gettext gettext-devel libtool unzip
#
#


CUR="$PWD"
LIBFUSE_DIR=$CUR/LIBFUSE

rm -rf libfuse
rm -rf $LIBFUSE_DIR

# please download https://gitee.com/mirrors/libfuse/repository/archive/fuse-2.9.9.zip
if ! [ -e ../ExFAT/mirrors-libfuse-fuse-2.9.9.zip ]; then
    echo "Please download mirrors-libfuse-fuse-2.9.9.zip first"
    exit 1
fi

unzip ../ExFAT/mirrors-libfuse-fuse-2.9.9.zip


cd libfuse
./makeconf.sh

./configure --prefix="$LIBFUSE_DIR"
make -j 16
make install
cd ..
rm -rf libfuse
