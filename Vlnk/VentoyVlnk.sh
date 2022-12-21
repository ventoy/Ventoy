#!/bin/sh

print_usage() {    
    echo 'Usage:  sudo sh VentoyVlnk.sh CMD FILE'
    echo '  CMD:'
    echo '   -c FILE      create vlnk for FILE'
    echo '   -l VLNK      parse vlnk file'
    echo '   -v           print verbose info'
    echo '   -h           print this help'
    echo ''
}

verbose_flag=0

vlog() {
    if [ $verbose_flag -eq 1 ]; then
        echo "$@"
    fi
}

vlnk_suffix() {
    echo $1 | grep -E -q '.*(.vlnk.iso|.vlnk.img|.vlnk.wim|.vlnk.vhd|.vlnk.vhdx|.vlnk.efi|.vlnk.vtoy|.vlnk.dat)$'
}


uid=$(id -u)
if [ $uid -ne 0 ]; then
    echo "Please use sudo or run the script as root."
    exit 1
fi

#check system tools used bellow
for t in 'mountpoint' 'readlink' 'xzcat'; do
    if ! which "$t" > /dev/null 2>&1; then
        echo "$t command not found in current system!"
        exit 1
    fi
done

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

fullsh=$(readlink -f "$0")
vtoydir=${fullsh%/*}

if [ -f "$vtoydir/tool/$TOOLDIR/vlnk.xz" ]; then
    xzcat "$vtoydir/tool/$TOOLDIR/vlnk.xz" > "$vtoydir/tool/$TOOLDIR/vlnk"
    rm -f "$vtoydir/tool/$TOOLDIR/vlnk.xz"
fi

if [ -f "$vtoydir/tool/$TOOLDIR/vlnk" ]; then
    chmod +x "$vtoydir/tool/$TOOLDIR/vlnk"
else
    echo "$vtoydir/tool/$TOOLDIR/vlnk does not exist! "
    exit 1
fi

PATH="$vtoydir/tool/$TOOLDIR":$PATH

VLNKCMD=vlnk
while [ -n "$1" ]; do
    if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        print_usage
        exit 0
    elif [ "$1" = "-c" ]; then
        shift
        CMD='c'
        IMG="$1"
    elif [ "$1" = "-o" ]; then
        shift
        OUT="$1"
    elif [ "$1" = "-l" ]; then
        shift
        CMD='l'
        VLNK="$1"
    elif [ "$1" = "-v" ]; then
        verbose_flag=1
        VLNKCMD="vlnk -v"
    else
        echo "invalid option $1"
        exit 1
    fi
    
    shift
done

if [ "$CMD" = "c" ]; then    
    vlog "Create vlnk for $IMG"
    
    if [ ! -f "$IMG" ]; then
        echo "$IMG does not exist!"
        exit 1
    fi
    
    if echo $IMG | grep -E -q -i '.*(.iso|.img|.wim|.vhd|.vhdx|.efi|.vtoy|.dat)$'; then
        :
    else
        echo "This file is not supported for vlnk!"
        exit 1
    fi
    
    if vlnk_suffix "$IMG"; then
        echo "This is already a vlnk file!"
        exit 1
    fi
    
    if $VLNKCMD -t "$IMG"; then
        echo "This is already a vlnk file!"
        exit 1
    fi
    
    FULLIMG=$(readlink -f "$IMG")
    if [ ! -f "$FULLIMG" ]; then
        echo "$FULLIMG does not exist!"
        exit 1
    fi
    vlog "Full file path is $FULLIMG"
    
    
    #check img file position is a valid mountpoint
    FULLDIR=${FULLIMG%/*}
    while [ -n "${FULLDIR}" ]; do
        if mountpoint -q "${FULLDIR}"; then
            break
        fi        
        FULLDIR="${FULLDIR%/*}"
    done
    
    if [ -z "${FULLDIR}" ]; then
        FULLDIR=/
        IMGPATH="${FULLIMG}"
    else
        IMGPATH="${FULLIMG#$FULLDIR}"
    fi
    
    IMGFILE=$(basename "$IMGPATH")
    vlog "IMGPATH=$IMGPATH IMGFILE=$IMGFILE"
    
    
    mntdev=$(mountpoint -d "${FULLDIR}")
    vlog "mountpoint is ${FULLDIR}  dev $mntdev"
    
    #check fs
    if grep -q " ${FULLDIR} " /proc/mounts; then
        DEV=$(grep " ${FULLDIR} " /proc/mounts | awk '{print $1}')
        FS=$(grep " ${FULLDIR} " /proc/mounts | awk '{print $3}')
        vlog "File system of $DEV is $FS"
        
        if echo $FS | grep -E -q "ext2|ext3|ext4|exfat|vfat|fat32|fat16|fat12|ntfs|xfs|udf"; then
            vlog "FS OK"
        elif [ "$FS" = "fuseblk" ]; then
            vlog "$DEV is fuseblk"
            if hexdump -C -n 8 $DEV | grep -q "NTFS"; then
                vlog "$DEV is NTFS OK"
            elif hexdump -C -n 8 $DEV | grep -q "EXFAT"; then
                vlog "$DEV is exFAT OK"
            else
                echo "$DEV is not supported!"
                hexdump -C -n 8 $DEV
                exit 1
            fi
        else
            echo "$FS is not supported!"
            exit 1
        fi
    else
        echo "${FULLDIR} not found in /proc/mounts"
        exit 1
    fi
    
    
    Major=$(echo $mntdev | awk -F: '{print $1}')
    Minor=$(echo $mntdev | awk -F: '{print $2}')
    vlog "Major=$Major Minor=$Minor"
    
    IMGPARTITION=""
    while read line; do
        M1=$(echo $line | awk '{print $1}')
        M2=$(echo $line | awk '{print $2}')        
        if [ "$Major" = "$M1" -a "$Minor" = "$M2" ]; then
            IMGPARTITION=$(echo $line | awk '{print $4}')
            vlog "disk partition is $IMGPARTITION"
            break
        fi
    done < /proc/partitions
    
    if [ -z "$IMGPARTITION" ]; then
        echo "Disk partition not found for $FULLDIR"
        grep " $FULLDIR " /proc/mounts
        exit 1
    fi
    
    if [ -f "/sys/class/block/$IMGPARTITION/start" ]; then
        PARTSTART=$(cat "/sys/class/block/$IMGPARTITION/start")
        if echo $IMGPARTITION | grep -E -q 'mmc|nbd|nvme'; then
            DISK=$(echo /dev/$IMGPARTITION | sed "s/^\(.*\)p[0-9][0-9]*$/\1/")
        else
            DISK=$(echo /dev/$IMGPARTITION | sed "s/^\(.*[^0-9]\)[0-9][0-9]*$/\1/")
        fi
        
        if [ ! -b $DISK ]; then
            echo "Device $DISK not exist!"
            exit 1
        fi
        
        vlog "PARTSTART=$PARTSTART DISK=$DISK"
    else
        if echo $IMGPARTITION | grep -q '^dm-[0-9][0-9]*'; then
            echo "LVM/DM is not supported!"
        fi
        echo "/sys/class/block/$IMGPARTITION/start not exist!"
        exit 1
    fi
    
    
    if [ -n "$OUT" ]; then
        lowersuffix=$(echo ${IMG##*.} | tr 'A-Z' 'a-z')        
        OUT="${OUT}.vlnk.${lowersuffix}"
    else
        name=${IMGFILE%.*}
        lowersuffix=$(echo ${IMGFILE##*.} | tr 'A-Z' 'a-z')
        OUT="${name}.vlnk.${lowersuffix}"
    fi

    echo "Output VLNK file is $OUT"
    [ -f "${OUT}" ] && rm -f "${OUT}"
    
    touch "${OUT}"
    if [ -f "${OUT}" ]; then
        rm -f "${OUT}"
    else
        echo "Failed to create ${OUT}"
        exit 1
    fi
    
    if $VLNKCMD -c "$IMGPATH" -d $DISK -p $PARTSTART -o "${OUT}"; then
        echo "====== Vlnk file create success ========="
        echo ""
    else
        echo "====== Vlnk file create failed ========="
        echo ""
        exit 1
    fi
elif [ "$CMD" = "l" ]; then
    vlog "Parse vlnk for $VLNK"
    
    if [ ! -f "$VLNK" ]; then
        echo "$VLNK does not exist!"
        exit 1
    fi

    if vlnk_suffix "$VLNK"; then
        :
    else
        echo "Invalid vlnk file suffix!"
        exit 1
    fi

    if $VLNKCMD -t "$VLNK"; then
        vlog "Vlnk data check OK"
    else
        echo "This is not a valid vlnk file!"
        exit 1
    fi

    $VLNKCMD -l "$VLNK"
    echo ""
else
    echo "invalid cmd"
    print_usage
    exit 1
fi
