#!/bin/sh

rm -f init

gcc -DMOUNT_NOMAIN  -Os -static *.c  -I. -lutil -lkiconv -o init

strip --strip-all init

if [ -e init ]; then
    echo -e "\n========= SUCCESS ==============\n"
else
    echo -e "\n========= FAILED ==============\n"
fi
