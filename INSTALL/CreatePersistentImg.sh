#!/bin/bash

size=1024
fstype=ext4
label=casper-rw
config=''
outputfile=persistence.dat

print_usage() {
    echo 'Usage:  sudo ./CreatePersistentImg.sh [ -s size ] [ -t fstype ] [ -l LABEL ] [ -c CFG ] [ -e ]'
    echo '  OPTION: (optional)'
    echo '   -s size in MB, default is 1024'
    echo '   -t filesystem type, default is ext4  ext2/ext3/ext4/xfs are supported now'
    echo '   -l label, default is casper-rw'
    echo '   -c configfile name inside the persistence file. File content is "/ union"'
    echo '   -o outputfile name, default is persistence.dat'
    echo '   -e enable encryption, disabled by default (only few distros support this)'    
    echo ''
}

print_err() {
    echo ""
    echo "$*"
    echo ""
}

uid=$(id -u)
if [ $uid -ne 0 ]; then
    print_err "Please use sudo or run the script as root."
    exit 1
fi

while [ -n "$1" ]; do
    if [ "$1" = "-s" ]; then
        shift
        size=$1
    elif [ "$1" = "-t" ]; then
        shift
        fstype=$1
    elif [ "$1" = "-l" ]; then
        shift
        label=$1
    elif [ "$1" = "-c" ]; then
        shift
        config=$1
    elif [ "$1" = "-o" ]; then
        shift
        outputfile=$1
    elif [ "$1" = "-e" ]; then
        read -s -p "Encryption passphrase: " passphrase
        echo
    elif [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        print_usage
        exit 0
    else
        print_usage
        exit 1
    fi
    shift
done


# check label
if [ -z "$label" ]; then
    echo "The label can NOT be empty."
    exit 1
fi

# check size
if echo $size | grep -q "^[0-9][0-9]*$"; then
    vtMinSize=1
    if echo $fstype | grep -q '^xfs$'; then
        vtMinSize=16
    fi
    
    if [ $size -lt $vtMinSize ]; then
        echo "size too small ($size)"
        exit 1
    fi    
else
    echo "Invalid size $size"
    exit 1
fi


# check file system type
# nodiscard must be set for ext2/3/4
# -K must be set for xfs 
if echo $fstype | grep -q '^ext[234]$'; then
    fsopt='-E nodiscard'
elif [ "$fstype" = "xfs" ]; then
    fsopt='-K'
else
    echo "unsupported file system $fstype"
    exit 1
fi

if [ "$outputdir" != "persistence.dat" ]; then
    mkdir -p "$(dirname "$outputfile")"
fi

# 00->ff avoid sparse file
dd if=/dev/zero  bs=1M count=$size | tr '\000' '\377' > "$outputfile"
sync

freeloop=$(losetup -f)

losetup $freeloop "$outputfile"

if [ ! -z "$passphrase" ]; then
    printf "$passphrase" | cryptsetup -q --verbose luksFormat $freeloop -
    printf "$passphrase" | cryptsetup -q --verbose luksOpen $freeloop persist_decrypted -
    _freeloop=$freeloop
    freeloop="/dev/mapper/persist_decrypted"
fi

mkfs -t $fstype $fsopt -L $label $freeloop 

sync

if [ -n "$config" ]; then
    if [ -d ./persist_tmp_mnt ]; then
        rm -rf ./persist_tmp_mnt
    fi
    
    mkdir ./persist_tmp_mnt
    if mount $freeloop ./persist_tmp_mnt; then
        echo '/ union' > ./persist_tmp_mnt/$config
        sync
        umount ./persist_tmp_mnt
    fi
    rm -rf ./persist_tmp_mnt
fi

if [ ! -z "$passphrase" ]; then
    cryptsetup luksClose $freeloop
    freeloop=$_freeloop
fi

losetup -d $freeloop
