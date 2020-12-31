#!/bin/bash

INITRD_SIZE=12
INITRD_FILE=dragonfly.mfs

rm -f ${INITRD_FILE}
rm -f ${INITRD_FILE}.xz

VN_DEV=$(vnconfig -c -S ${INITRD_SIZE}m -Z -T vn ${INITRD_FILE})
newfs -i 131072 -m 0 /dev/${VN_DEV}s0
mount_ufs /dev/${VN_DEV}s0 /mnt

cp -a sbin /mnt/
chmod -R 777 /mnt/sbin

mkdir /mnt/dev
mkdir /mnt/new_root
mkdir /mnt/tmp

dd if=/dev/zero of=./data bs=1M count=8

cat ./dmtable ./data ./dmtable > /mnt/dmtable 

umount /mnt

rm -f ./data

xz ${INITRD_FILE}

vnconfig -u ${VN_DEV}
