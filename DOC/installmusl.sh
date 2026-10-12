#!/bin/bash

if ! [ -f ./musl-1.2.1.tar.gz ]; then
    echo "No musl-1.2.1.tar.gz found ..."
    exit 1
fi

echo "======= install musl =============="

tar xf musl-1.2.1.tar.gz
cd musl-1.2.1
./configure && make install

cd ..
rm -rf musl-1.2.1

echo ""
echo " ================ success ==============="
echo ""

