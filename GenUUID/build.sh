#!/bin/bash

/opt/diet32/bin/diet gcc -Os -m32 vtoy_gen_uuid.c -o  vtoy_gen_uuid

if [ -e vtoy_gen_uuid ]; then
    echo -e '\n############### SUCCESS ###############\n'

    rm -f ../INSTALL/tool/vtoy_gen_uuid
    cp -a vtoy_gen_uuid ../INSTALL/tool/vtoy_gen_uuid
else
    echo -e '\n############### FAILED ################\n'
    exit 1
fi

