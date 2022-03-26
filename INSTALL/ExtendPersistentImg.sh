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

if echo $size | grep -q "^-"; then
    mode="Shrink"
    size=${size:1}
else
    mode="Extend"
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

if [ "$mode" = "Extend" ]; then
    total=$(expr $fsMB + $size)
else
    if [ $fsMB -le $size ]; then
        echo "File size of $file is less than ${size}MB."
        exit 1
    fi
    total=$(expr $fsMB - $size)
fi


magic=$(hexdump -n3 -e  '3/1 "%02X"' $file)
if [ "$magic" = "584653" ]; then
    if [ "$mode" = "Shrink" ]; then
        echo "Shrink is not supported for XFS filesystem."
        exit 1
    fi

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

if [ "$mode" = "Extend" ]; then
    echo "$mode dat file... (current is ${fsMB}MB, append ${size}MB, total ${total}MB)"
    dd if=/dev/zero bs=1M count=$size status=none >> "$file"
    sync
else
    echo "$mode dat file... (current is ${fsMB}MB, reduce ${size}MB, finally ${total}MB)"
fi


freeloop=$(losetup -f)
losetup $freeloop "$file"

if [ "$cmd" = "resize2fs" ]; then    
    echo "$mode ext filesystem by resize2fs ..."
    echo "resize2fs $freeloop ${total}M"
    e2fsck -f $freeloop
    resize2fs $freeloop ${total}M
    ret=$?
else
    echo "$mode xfs filesystem by xfs_growfs ..."
    tmpdir=$(mktemp -d)
    mount $freeloop $tmpdir
    xfs_growfs $freeloop
    ret=$?
    umount $tmpdir && rm -rf $tmpdir
fi

losetup -d $freeloop

if [ $ret -eq 0 -a "$mode" = "Shrink" ]; then
    echo "truncate persistent file ..."
    truncate "$file" -s ${total}M
    ret=$?
fi

echo ""
if [ $ret -eq 0 ]; then
    echo "======= SUCCESS ========="
else
    echo "======= FAILED ========="
fi
echo ""
