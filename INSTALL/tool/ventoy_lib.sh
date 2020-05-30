#!/bin/sh

#Ventoy partition 32MB
VENTOY_PART_SIZE=33554432
VENTOY_SECTOR_SIZE=512
VENTOY_SECTOR_NUM=65536

ventoy_false() {
    [ "1" = "2" ]
}

ventoy_true() {
    [ "1" = "1" ]
}

ventoy_is_linux64() {
    if uname -a | egrep -q 'x86_64|amd64'; then
        ventoy_true
        return
    fi
    
    ventoy_false
}

vtinfo() {
    echo -e "\033[32m$*\033[0m"
}

vtwarn() {
    echo -e "\033[33m$*\033[0m"
}


vterr() {
    echo -e "\033[31m$*\033[0m"
}

vtdebug() {
    echo "$*" >> ./log.txt
}

check_tool_work_ok() {
    
    if ventoy_is_linux64; then
        vtdebug "This is linux 64"
        mkexfatfs=mkexfatfs_64
        vtoyfat=vtoyfat_64
    else
        vtdebug "This is linux 32"
        mkexfatfs=mkexfatfs_32
        vtoyfat=vtoyfat_32
    fi
    
    if echo 1 | ./tool/hexdump > /dev/null; then
        vtdebug "hexdump test ok ..."
    else
        vtdebug "hexdump test fail ..."
        ventoy_false
        return
    fi
   
    if ./tool/$mkexfatfs -V > /dev/null; then
        vtdebug "$mkexfatfs test ok ..."
    else
        vtdebug "$mkexfatfs test fail ..."
        ventoy_false
        return
    fi
    
    if ./tool/$vtoyfat -T; then
        vtdebug "$vtoyfat test ok ..."
    else
        vtdebug "$vtoyfat test fail ..."
        ventoy_false
        return
    fi
    
    vtdebug "tool check success ..."
    ventoy_true
}


get_disk_part_name() {
    DISK=$1
    
    if echo $DISK | grep -q "/dev/loop"; then
        echo ${DISK}p${2}
    elif echo $DISK | grep -q "/dev/nvme[0-9][0-9]*n[0-9]"; then
        echo ${DISK}p${2}
    else
        echo ${DISK}${2}
    fi
}


get_ventoy_version_from_cfg() {
    if grep -q 'set.*VENTOY_VERSION=' $1; then
        grep 'set.*VENTOY_VERSION=' $1 | awk -F'"' '{print $2}'
    else
        echo 'none'
    fi
}

is_disk_contains_ventoy() {
    DISK=$1
    
    PART1=$(get_disk_part_name $1 1)  
    PART2=$(get_disk_part_name $1 2)  
    
    if [ -e /sys/class/block/${PART2#/dev/}/size ]; then    
        SIZE=$(cat /sys/class/block/${PART2#/dev/}/size)
    else
        SIZE=0
    fi

    if ! [ -b $PART1 ]; then
        vtdebug "$PART1 not exist"
        ventoy_false
        return
    fi
    
    if ! [ -b $PART2 ]; then
        vtdebug "$PART2 not exist"
        ventoy_false
        return
    fi
    
    PART2_TYPE=$(dd if=$DISK bs=1 count=1 skip=466 status=none | ./tool/hexdump -n1 -e  '1/1 "%02X"')
    if [ "$PART2_TYPE" != "EF" ]; then
        vtdebug "part2 type is $PART2_TYPE not EF"
        ventoy_false
        return
    fi
    
    # PART1_TYPE=$(dd if=$DISK bs=1 count=1 skip=450 status=none | ./tool/hexdump -n1 -e  '1/1 "%02X"')
    # if [ "$PART1_TYPE" != "07" ]; then
        # vtdebug "part1 type is $PART2_TYPE not 07"
        # ventoy_false
        # return
    # fi
    
    if [ -e /sys/class/block/${PART1#/dev/}/start ]; then
        PART1_START=$(cat /sys/class/block/${PART1#/dev/}/start)
    fi
    
    if [ "$PART1_START" != "2048" ]; then
        vtdebug "part1 start is $PART1_START not 2048"
        ventoy_false
        return
    fi 
    
    if [ "$VENTOY_SECTOR_NUM" != "$SIZE" ]; then
        vtdebug "part2 size is $SIZE not $VENTOY_SECTOR_NUM"
        ventoy_false
        return
    fi
    
    ventoy_true
}

get_disk_ventoy_version() {

    if ! is_disk_contains_ventoy $1; then
        ventoy_false
        return
    fi
    
    PART2=$(get_disk_part_name $1 2)    
    
    if ventoy_is_linux64; then
        cmd=./tool/vtoyfat_64
    else
        cmd=./tool/vtoyfat_32
    fi
    
    ParseVer=$($cmd $PART2)
    if [ $? -eq 0 ]; then
        vtdebug "Ventoy version in $PART2 is $ParseVer"
        echo $ParseVer
        ventoy_true
        return
    fi
    
    ventoy_false
}

format_ventoy_disk() {
    DISK=$1
    PARTTOOL=$2
    
    PART1=$(get_disk_part_name $DISK 1)
    PART2=$(get_disk_part_name $DISK 2)
    
    sector_num=$(cat /sys/block/${DISK#/dev/}/size)
    
    part1_start_sector=2048
    part1_end_sector=$(expr $sector_num - $VENTOY_SECTOR_NUM - 1)
    export part2_start_sector=$(expr $part1_end_sector + 1)
    part2_end_sector=$(expr $sector_num - 1)

    vtdebug "part1_start_sector=$part1_start_sector  part1_end_sector=$part1_end_sector"
    vtdebug "part2_start_sector=$part2_start_sector  part2_end_sector=$part2_end_sector"

    if [ -e $PART2 ]; then
        echo "delete $PART2"
        rm -f $PART2
    fi

    echo ""
    echo "Create partitions on $DISK by $PARTTOOL ..."
    
    if [ "$PARTTOOL" = "parted" ]; then
        vtdebug "format disk by parted ..."
        parted -a none --script $DISK \
            mklabel msdos \
            unit s \
            mkpart primary ntfs $part1_start_sector $part1_end_sector \
            mkpart primary fat16 $part2_start_sector $part2_end_sector \
            set 1 boot on \
            quit

        sync
        echo -en '\xEF' | dd of=$DISK conv=fsync bs=1 count=1 seek=466 > /dev/null 2>&1
    else
    vtdebug "format disk by fdisk ..."
    
fdisk $DISK >>./log.txt 2>&1 <<EOF
o
n
p
1
$part1_start_sector
$part1_end_sector
n
p
2
$part2_start_sector
$part2_end_sector
t
1
7
t
2
ef
a
1
w
EOF
    fi
   
    udevadm trigger >/dev/null 2>&1
    partprobe >/dev/null 2>&1
    sleep 3
    echo "Done"

    echo 'mkfs on disk partitions ...'
    for i in 1 2 3 4 5 6 7; do
        if [ -b $PART2 ]; then
            break
        else
            echo "wait $PART2 ..."
            sleep 1
        fi
    done


    if ! [ -b $PART2 ]; then
        MajorMinor=$(sed "s/:/ /" /sys/class/block/${PART2#/dev/}/dev)        
        echo "mknod -m 0660 $PART2 b $MajorMinor ..."
        mknod -m 0660 $PART2 b $MajorMinor
        
        if ! [ -b $PART1 ]; then
            MajorMinor=$(sed "s/:/ /" /sys/class/block/${PART1#/dev/}/dev)        
            echo "mknod -m 0660 $PART1 b $MajorMinor ..."
            mknod -m 0660 $PART1 b $MajorMinor
        fi
    fi

    echo "create efi fat fs $PART2 ..."
    for i in 0 1 2 3 4 5 6 7 8 9; do
        if mkfs.vfat -F 16 -n EFI $PART2; then
            echo 'success'
            break
        else
            echo "$? retry ..."
            sleep 2
        fi
    done
}




