#!/bin/bash

. ./tool/ventoy_lib.sh

print_usage() {    
    echo 'Usage:  sudo bash VentoyPlugson.sh [OPTION] /dev/sdX'
    echo '  OPTION: (optional)'
    echo '   -H x.x.x.x  http server IP address (default is 127.0.0.1)'
    echo '   -P PORT     http server PORT (default is 24681)'
    echo '   -h          print this help'
    echo ''
}

uid=$(id -u)
if [ $uid -ne 0 ]; then
    echo "Please use sudo or run the script as root."
    exit 1
fi

if [ "$1" = "__vbash__" ]; then
    shift
else
    if readlink /bin/sh | grep -q bash; then
        :
    else
        exec /bin/bash $0 "__vbash__" "$@"
    fi
fi

OLDDIR=$(pwd)

machine=$(uname -m)
if echo $machine | grep -E -q 'aarch64|arm64'; then
    TOOLDIR=aarch64
elif echo $machine | grep -E -q 'x86_64|amd64'; then
    TOOLDIR=x86_64
elif echo $machine | grep -E -q 'mips64'; then
    TOOLDIR=mips64el
elif echo $machine | grep -E -q 'i[3-6]86'; then
    TOOLDIR=i386
else
    echo "Unsupported machine type $machine"    
    exit 1
fi


if ! [ -f "$OLDDIR/tool/plugson.tar.xz" ]; then
    echo "Please run under the correct directory!" 
    exit 1
fi

echo "############# VentoyPlugson $* [$TOOLDIR] ################" >> ./VentoyPlugson.log
date >> ./VentoyPlugson.log

echo "decompress tools" >> ./VentoyPlugson.log
cd ./tool/$TOOLDIR

ls *.xz > /dev/null 2>&1
if [ $? -eq 0 ]; then
    [ -f ./xzcat ] && chmod +x ./xzcat

    for file in $(ls *.xz); do
        echo "decompress $file" >> ./VentoyPlugson.log
        xzcat $file > ${file%.xz}
        [ -f ./${file%.xz} ] && chmod +x ./${file%.xz}
        [ -f ./$file ] && rm -f ./$file
    done
fi

cd ../../
chmod +x -R ./tool/$TOOLDIR

if ! [ -f "$OLDDIR/tool/$TOOLDIR/Plugson" ]; then
    echo "$OLDDIR/tool/$TOOLDIR/Plugson does not exist!" 
    exit 1
fi


PATH=./tool/$TOOLDIR:$PATH

HOST="127.0.0.1"
PORT=24681

while [ -n "$1" ]; do
    if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        print_usage
        exit 0
    elif [ "$1" = "-H" ]; then
        shift
        if echo $1 | grep -q '[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*'; then
            HOST="$1"
        else
            echo "Invalid host $1"
            exit 1
        fi
    elif [ "$1" = "-P" ]; then
        shift
        if [ $1 -gt 0 -a $1 -le 65535 ]; then
            PORT="$1"
        else
            echo "Invalid port $1"
            exit 1
        fi
    else
        DISK=$1
    fi
    
    shift
done

if [ -z "$DISK" ]; then
    print_usage
    exit 0
fi

if ps -ef | grep "tool/$TOOLDIR/Plugson.*$HOST.*$PORT" | grep -q -v grep; then
    echo "Another ventoy server is running now, please close it first."
    exit 1
fi

if echo $DISK | grep -q "[a-z]d[a-z][1-9]"; then
    DISK=${DISK:0:-1}
fi

if echo $DISK | grep -E -q "/dev/nvme|/dev/mmcblk/dev/nbd"; then
    if echo $DISK | grep -q "p[1-9]$"; then
        DISK=${DISK:0:-2}
    fi
fi


if [ ! -b "$DISK" ]; then
    echo "$DISK does NOT exist."
    exit 1
fi


version=$(get_disk_ventoy_version $DISK)
if [ $? -eq 0 ]; then
    echo "Ventoy version in Disk: $version"
    
    vtPart1Type=$(dd if=$DISK bs=1 count=1 skip=450 status=none | hexdump -n1 -e  '1/1 "%02X"')
    if [ "$vtPart1Type" = "EE" ]; then            
        echo "Disk Partition Style  : GPT"
        partstyle=1
    else
        echo "Disk Partition Style  : MBR"
        partstyle=0
    fi

    if check_disk_secure_boot $DISK; then
        echo "Secure Boot Support   : YES"
        secureboot=1
    else
        echo "Secure Boot Support   : NO"
        secureboot=0
    fi
else
    echo "$DISK is NOT Ventoy disk."
    exit 1
fi

PART1=$(get_disk_part_name $DISK 1)

if grep -q "^$PART1 " /proc/mounts; then
    mtpnt=$(grep "^$PART1 " /proc/mounts | awk '{print $2}' | sed 's/\\040/ /g')
    fstype=$(grep "^$PART1 " /proc/mounts | awk '{print $3}')
    
    if echo $fstype | grep -q -i 'fuse'; then
        if hexdump -C -n 16 $PART1 | grep -q -i "EXFAT"; then
            fstype="exFAT"
        elif hexdump -C -n 16 $PART1 | grep -q -i "NTFS"; then
            fstype="NTFS"       
        fi
    fi
    
    echo "$PART1 is mounted at $mtpnt $fstype"
else
    echo "$PART1 is NOT mounted, please mount it first!"
    exit 1
fi

if [ -d "$mtpnt/ventoy" ]; then
    echo "ventoy directory exist OK"
else
    echo "create ventoy directory"
    mkdir -p "$mtpnt/ventoy"
    if [ -d "$mtpnt/ventoy" ]; then
        chmod -R 0755 "$mtpnt/ventoy"
    else
        echo "Failed to create directory $mtpnt/ventoy"
        exit 1
    fi
fi


#change current directory to Ventoy disk
cd "$mtpnt"
"$OLDDIR/tool/$TOOLDIR/Plugson" "$HOST" "$PORT" "$OLDDIR" "$DISK" $version "$fstype" $partstyle $secureboot   &
wID=$!
sleep 1

if [ -f /proc/$wID/maps ]; then
    echo ""
    echo "==============================================================="
    if [ "$LANG" = "zh_CN.UTF-8" ]; then
        echo "  Ventoy Plugson Server 已经启动 ..."
        echo "  请打开浏览器，访问 http://${HOST}:${PORT}"
    else
        echo "  Ventoy Plugson Server is running ..."
        echo "  Please open your browser and visit http://${HOST}:${PORT}"
    fi
    echo "==============================================================="
    echo ""
    echo "################## Press Ctrl + C to exit #####################"
    echo ""

    wait $wID
fi


if [ -n "$OLDDIR" ]; then 
    CURDIR=$(pwd)
    if [ "$CURDIR" != "$OLDDIR" ]; then
        cd "$OLDDIR"
    fi
fi
