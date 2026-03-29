#!/bin/sh

if [ "$(id -u)" -ne 0 ]; then    
    echo "Please run with sudo ..."
    exit 1
fi

oldpwd=$(pwd)
VPART=""

if dmsetup -h > /dev/null 2>&1; then
    VPART_MAJOR_MINOR=$(dmsetup table ventoy | head -n 1 | awk '{print $4}')    
    cd /sys/class/block/
    for t in *; do
        if grep -q "^${VPART_MAJOR_MINOR}$" $t/dev; then
            VPART=$t
            echo 0 $(cat /sys/class/block/$VPART/size) linear /dev/$VPART 0 | dmsetup create $VPART
            dmsetup mknodes "$VPART" > /dev/null 2>&1
            break
        fi
    done
    cd $oldpwd
    
    if [ -z "$VPART" ]; then
        echo "$VPART_MAJOR_MINOR not found"
        dmsetup ls; dmsetup info ventoy; dmsetup table ventoy
        exit 1
    else                                 
        if [ ! -b "/dev/mapper/$VPART" ]; then
            udevadm trigger --type=devices --action=add  > /dev/null 2>&1
            udevadm settle > /dev/null 2>&1
        fi
        echo "Create /dev/mapper/$VPART success"
    fi    
else
    echo "dmsetup program not avaliable"
    exit 1
fi
