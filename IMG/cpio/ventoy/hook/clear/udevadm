#!/bin/bash

UPATH=/usr/bin
DM=dm-0

rm -f $UPATH/udevadm
mv $UPATH/udevadm_bk $UPATH/udevadm

echo 1 > /tmp/vthidden
mount --bind /tmp/vthidden /sys/block/$DM/hidden

exec $UPATH/udevadm "$@"
