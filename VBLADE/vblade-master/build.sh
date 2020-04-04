#!/bin/bash

rm -f vblade_*

gcc linux.c aoe.c ata.c bpf.c -Os -o vblade_64
gcc linux.c aoe.c ata.c bpf.c -Os -m32 -o vblade_32

if [ -e vblade_64 ] && [ -e vblade_32 ]; then
    echo -e '\n################## SUCCESS ######################\n'
else
    echo -e '\n################## FAILED ######################\n'
    exit 1
fi

