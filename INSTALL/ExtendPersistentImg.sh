#!/bin/sh

print_usage() {
    echo 'Usage:  ExtendPersistentImg.sh file size'
    echo '   file   persistent dat file'
    echo '   size   extend size in MB'
    echo 'Example:'
    echo '   sh ExtendPersistentImg.sh ubuntu.dat 2048'
    echo ''
}

if [ -z "$1" -o "$1" = "-h" ]; then
    print_usage
    exit 1
fi

if [ -z "$2" ]; then
    print_usage
    exit 1
fi

file=$1
size=$2

if [ ! -f "$file" ]; then
    echo "$file not exist."
    exit 1
fi

if echo $size | grep -q "[^0-9]"; then
    print_usage
    exit 1
fi

fsize=$(stat -c '%s' $file)

fsmod=$(expr $fsize % 1024)
if [ $fsmod -ne 0 ]; then
    echo "File size of $file is not aligned by 1MB, please check."
    exit 1
fi


fsMB=$(expr $fsize / 1024 / 1024)
total=$(expr $fsMB + $size)

magic=$(hexdump -n3 -e  '3/1 "%02X"' $file)
if [ "$magic" = "584653" ]; then
    if which xfs_growfs >/dev/null 2>&1; then
        cmd=xfs_growfs
    else
        echo 'xfs_growfs not found, please install xfsprogs first'
        exit 1
    fi
else
    if which resize2fs >/dev/null 2>&1; then
        cmd=resize2fs
    else
        echo 'resize2fs not found, please install e2fsprogs first'
        exit 1
    fi
fi


echo "Extend dat file... (current is ${fsMB}MB, append ${size}MB, total ${total}MB)"
dd if=/dev/zero bs=1M count=$size status=none >> "$file"
sync

freeloop=$(losetup -f)
losetup $freeloop "$file"

if [ "$cmd" = "resize2fs" ]; then    
    echo "Extend ext filesystem by resize2fs ..."
    echo "resize2fs $freeloop ${total}M"
    e2fsck -f $freeloop
    resize2fs $freeloop ${total}M
    ret=$?
else
    echo "Extend xfs filesystem by xfs_growfs ..."

    tmpdir=$(mktemp -d)
    mount $freeloop $tmpdir
    xfs_growfs $freeloop
    ret=$?
    umount $tmpdir && rm -rf $tmpdir
fi

losetup -d $freeloop

echo ""
if [ $ret -eq 0 ]; then
    echo "======= SUCCESS ========="
else
    echo "======= FAILED ========="
fi
echo ""

