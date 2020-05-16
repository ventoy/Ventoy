#!/bin/sh

OLDDIR=$PWD

if ! [ -f ./tool/ventoy_lib.sh ]; then
    cd ${0%Ventoy2Disk.sh}
fi

. ./tool/ventoy_lib.sh

print_usage() {
    echo 'Usage:  Ventoy2Disk.sh CMD [ OPTION ] /dev/sdX'
    echo '  CMD:'
    echo '   -i  install ventoy to sdX (fail if disk already installed with ventoy)'
    echo '   -u  update ventoy in sdX'
    echo '   -I  force install ventoy to sdX (no matter installed or not)'
    echo ''
    echo '  OPTION: (optional)'
    echo '   -s  enable secure boot support (default is disabled)'
    echo ''
    
}

echo ''
echo '***********************************************************'
echo '*                Ventoy2Disk Script                       *'
echo '*             longpanda  admin@ventoy.net                 *'
echo '***********************************************************'
echo ''

vtdebug "############# Ventoy2Disk $0 ################"

while [ -n "$1" ]; do
    if [ "$1" = "-i" ]; then
        MODE="install"
    elif [ "$1" = "-I" ]; then
        MODE="install"
        FORCE="Y"
    elif [ "$1" = "-u" ]; then
        MODE="update"
    elif [ "$1" = "-s" ]; then
        SECUREBOOT="YES"
    else
        if ! [ -b "$1" ]; then
            vterr "$1 is NOT a valid device"
            print_usage
            cd $OLDDIR
            exit 1
        fi
        DISK=$1
    fi
    
    shift
done

if [ -z "$MODE" ]; then
    print_usage
    cd $OLDDIR
    exit 1
fi

if ! [ -b "$DISK" ]; then
    vterr "Disk $DISK does not exist"
    cd $OLDDIR
    exit 1
fi

if [ -e /sys/class/block/${DISK#/dev/}/start ]; then
    vterr "$DISK is a partition, please use the whole disk"
    cd $OLDDIR
    exit 1
fi

if dd if="$DISK" of=/dev/null bs=1 count=1 >/dev/null 2>&1; then
    vtdebug "root permission check ok ..."
else
    vterr "Failed to access $DISK, maybe root privilege is needed!"
    echo ''
    cd $OLDDIR
    exit 1
fi

vtdebug "MODE=$MODE FORCE=$FORCE"

if ! [ -f ./boot/boot.img ]; then
    if [ -d ./grub ]; then
        vterr "Don't run me here, please download the released install package, and run there."
    else
        vterr "Please run under the right directory!" 
    fi
    exit 1
fi

#decompress tool
cd tool
chmod +x ./xzcat
for file in $(ls); do
    if [ "$file" != "xzcat" ]; then
        if [ "$file" != "ventoy_lib.sh" ]; then
            ./xzcat $file > ${file%.xz}
            chmod +x ${file%.xz}
        fi
    fi
done
cd ../

if ! check_tool_work_ok; then
    vterr "Some tools can not run in current system. Please check log.txt for detail."
    cd $OLDDIR
    exit 1
fi

grep "^$DISK" /proc/mounts | while read mtline; do
    mtpnt=$(echo $mtline | awk '{print $DISK}')
    vtdebug "Trying to umount $mtpnt ..."
    umount $mtpnt >/dev/null 2>&1
done

if grep "$DISK" /proc/mounts; then
    vterr "$DISK is already mounted, please umount it first!"
    cd $OLDDIR
    exit 1
fi


if [ "$MODE" = "install" ]; then
    vtdebug "install ventoy ..."
    
    if ! fdisk -v >/dev/null 2>&1; then
        vterr "fdisk is needed by ventoy installation, but is not found in the system."
        cd $OLDDIR
        exit 1
    fi
    
    version=$(get_disk_ventoy_version $DISK)
    if [ $? -eq 0 ]; then
        if [ -z "$FORCE" ]; then
            vtwarn "$DISK already contains a Ventoy with version $version"
            vtwarn "Use -u option to do a safe upgrade operation."
            vtwarn "OR if you really want to reinstall ventoy to $DISK, please use -I option."
            vtwarn ""
            cd $OLDDIR
            exit 1
        fi
    fi
    
    disk_sector_num=$(cat /sys/block/${DISK#/dev/}/size)
    disk_size_gb=$(expr $disk_sector_num / 2097152)

    if [ $disk_sector_num -gt 4294967296 ]; then
        vterr "$DISK is over 2TB size, MBR will not work on it."
        cd $OLDDIR
        exit 1
    fi

    #Print disk info
    echo "Disk : $DISK"
    parted -s $DISK p 2>&1 | grep Model
    echo "Size : $disk_size_gb GB"
    echo ''

    vtwarn "Attention:"
    vtwarn "You will install Ventoy to $DISK."
    vtwarn "All the data on the disk $DISK will be lost!!!"
    echo ""

    read -p 'Continue? (y/n)'  Answer
    if [ "$Answer" != "y" ]; then
        if [ "$Answer" != "Y" ]; then
            exit 0
        fi
    fi

    echo ""
    vtwarn "All the data on the disk $DISK will be lost!!!"
    read -p 'Double-check. Continue? (y/n)'  Answer
    if [ "$Answer" != "y" ]; then
        if [ "$Answer" != "Y" ]; then
            exit 0
        fi
    fi


    if [ $disk_sector_num -le $VENTOY_SECTOR_NUM ]; then  
        vterr "No enough space in disk $DISK"
        exit 1
    fi

    if ! dd if=/dev/zero of=$DISK bs=1 count=512 status=none conv=fsync; then
        vterr "Write data to $DISK failed, please check whether it's in use."
        exit 1
    fi

    format_ventoy_disk $DISK

    # format part1
    if ventoy_is_linux64; then
        cmd=./tool/mkexfatfs_64
    else
        cmd=./tool/mkexfatfs_32
    fi

    chmod +x ./tool/*

    # DiskSize > 32GB  Cluster Size use 128KB
    # DiskSize < 32GB  Cluster Size use 32KB
    if [ $disk_size_gb -gt 32 ]; then
        cluster_sectors=256
    else
        cluster_sectors=64
    fi

    $cmd -n ventoy -s $cluster_sectors ${DISK}1

    chmod +x ./tool/vtoy_gen_uuid

    vtinfo "writing data to disk ..."
    dd status=none conv=fsync if=./boot/boot.img of=$DISK bs=1 count=446
    ./tool/xzcat ./boot/core.img.xz | dd status=none conv=fsync of=$DISK bs=512 count=2047 seek=1
    ./tool/xzcat ./ventoy/ventoy.disk.img.xz | dd status=none conv=fsync of=$DISK bs=512 count=$VENTOY_SECTOR_NUM seek=$part2_start_sector
    
    #disk uuid
    ./tool/vtoy_gen_uuid | dd status=none conv=fsync of=${DISK} seek=384 bs=1 count=16
    
    #disk signature
    ./tool/vtoy_gen_uuid | dd status=none conv=fsync of=${DISK} skip=12 seek=440 bs=1 count=4

    vtinfo "sync data ..."
    sync
    
    vtinfo "esp partition processing ..."
    
    if [ "$SECUREBOOT" != "YES" ]; then
        mkdir ./tmp_mnt
        
        vtdebug "mounting part2 ...."
        for tt in 1 2 3; do
            if mount ${DISK}2 ./tmp_mnt; then
                vtdebug "mounting part2 success"
                break
            fi
            sleep 2
        done
              
        rm -f ./tmp_mnt/EFI/BOOT/BOOTX64.EFI
        rm -f ./tmp_mnt/EFI/BOOT/grubx64.efi
        rm -f ./tmp_mnt/EFI/BOOT/MokManager.efi
        rm -f ./tmp_mnt/ENROLL_THIS_KEY_IN_MOKMANAGER.cer
        mv ./tmp_mnt/EFI/BOOT/grubx64_real.efi  ./tmp_mnt/EFI/BOOT/BOOTX64.EFI
        
        umount ./tmp_mnt
        rm -rf ./tmp_mnt
    fi

    echo ""
    vtinfo "Install Ventoy to $DISK successfully finished."
    echo ""
    
else
    vtdebug "update ventoy ..."
    
    oldver=$(get_disk_ventoy_version $DISK)
    if [ $? -ne 0 ]; then
        vtwarn "$DISK does not contain ventoy or data corupted"
        echo ""
        vtwarn "Please use -i option if you want to install ventoy to $DISK"
        echo ""
        cd $OLDDIR
        exit 1
    fi

    curver=$(cat ./ventoy/version)

    vtinfo "Upgrade operation is safe, all the data in the 1st partition (iso files and other) will be unchanged!"
    echo ""

    read -p "Update Ventoy  $oldver ===> $curver   Continue? (y/n)"  Answer
    if [ "$Answer" != "y" ]; then
        if [ "$Answer" != "Y" ]; then
            cd $OLDDIR
            exit 0
        fi
    fi

    PART2=$(get_disk_part_name $DISK 2)
    
    dd status=none conv=fsync if=./boot/boot.img of=$DISK bs=1 count=440
    
    ./tool/xzcat ./boot/core.img.xz | dd status=none conv=fsync of=$DISK bs=512 count=2047 seek=1  

    disk_sector_num=$(cat /sys/block/${DISK#/dev/}/size) 
    part2_start=$(expr $disk_sector_num - $VENTOY_SECTOR_NUM)
    ./tool/xzcat ./ventoy/ventoy.disk.img.xz | dd status=none conv=fsync of=$DISK bs=512 count=$VENTOY_SECTOR_NUM seek=$part2_start

    sync
    
    if [ "$SECUREBOOT" != "YES" ]; then
        mkdir ./tmp_mnt
        
        vtdebug "mounting part2 ...."
        for tt in 1 2 3; do
            if mount ${DISK}2 ./tmp_mnt; then
                vtdebug "mounting part2 success"
                break
            fi
            sleep 2
        done
              
        rm -f ./tmp_mnt/EFI/BOOT/BOOTX64.EFI
        rm -f ./tmp_mnt/EFI/BOOT/grubx64.efi
        rm -f ./tmp_mnt/EFI/BOOT/MokManager.efi
        rm -f ./tmp_mnt/ENROLL_THIS_KEY_IN_MOKMANAGER.cer
        mv ./tmp_mnt/EFI/BOOT/grubx64_real.efi  ./tmp_mnt/EFI/BOOT/BOOTX64.EFI
        
        umount ./tmp_mnt
        rm -rf ./tmp_mnt
    fi

    echo ""
    vtinfo "Update Ventoy to $DISK successfully finished."
    echo ""
    
fi

cd $OLDDIR

