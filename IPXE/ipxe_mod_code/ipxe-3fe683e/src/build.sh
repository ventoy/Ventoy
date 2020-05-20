#!/bin/bash

build_bios() {
    rm -f bin/ipxe.iso

    make -e -k -j 8 bin/ipxe.iso   BIOS_MODE=BIOS

    if ! [ -e bin/ipxe.iso ]; then
        echo "Failed"
        exit 1
    fi

    mkdir -p ./mnt
    mount bin/ipxe.iso ./mnt

    rm -f ../../../INSTALL/ventoy/ipxe.krn
    cp -a ./mnt/ipxe.krn ../../../INSTALL/ventoy/ipxe.krn        
    
    umount ./mnt > /dev/null 2>&1
    umount ./mnt > /dev/null 2>&1
    umount ./mnt > /dev/null 2>&1
    
    rm -rf ./mnt

    echo -e "\n===============SUCCESS===============\n"
}


build_bios


