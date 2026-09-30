#!/bin/sh

PATH=$PATH:/ventoy/busybox:/ventoy/tool

echo "$@"  >> /ventoy/log

VT_DMNAME=$1
VT_PART_NUM="${VT_DMNAME#*ventoy}"
VT_PARENT_DEV="/dev/mapper/ventoy"
VT_PARTUUID=

[ -b "$VT_PARENT_DEV" ] || exit 0

VT_PARTUUID=$(/ventoy/tool/vtoygpt $VT_PARENT_DEV $VT_PART_NUM | awk '{print $2}')

echo "$VT_DMNAME  PARTUUID=$VT_PARTUUID" >> /ventoy/log

if [ -n "$VT_PARTUUID" ]; then
    echo "ID_PART_ENTRY_UUID=$VT_PARTUUID"
    echo "ID_PART_ENTRY_NUMBER=$VT_PART_NUM"
    echo "ID_PART_ENTRY_SCHEME=gpt"
fi
