#!/bin/bash

rm -rf ipxe-3fe683e

tar -xvf ipxe_org_code/ipxe-3fe683e.tar.bz2 -C ./

rm -rf ./ipxe-3fe683e/src/bin
rm -rf ./ipxe-3fe683e/src/drivers

/bin/cp -a ipxe_mod_code/ipxe-3fe683e ./

cd ipxe-3fe683e/src

sh build.sh

cd ../../

