#!/ventoy/busybox/sh
#************************************************************************************
# Copyright (c) 2020, longpanda <admin@ventoy.net>
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
# 
#************************************************************************************

. /ventoy/hook/ventoy-hook-lib.sh

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH


ventoy_os_install_dmsetup_by_unsquashfs() {
    vtlog "ventoy_os_install_dmsetup_by_unsquashfs $*"
    
    vtKoPo=$(ventoy_get_module_postfix)
    vtlog "vtKoPo=$vtKoPo"

    vtoydm -i -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/iso_file_list

    vtline=$(grep '[-][-] linuxfs '  $VTOY_PATH/iso_file_list)    
    sector=$(echo $vtline | awk '{print $(NF-1)}')
    length=$(echo $vtline | awk '{print $NF}')
    
    vtoydm -E -f $VTOY_PATH/ventoy_image_map -d $1 -s $sector -l $length -o $VTOY_PATH/fsdisk
    
    dmModPath="/usr/lib/modules/$vtKerVer/kernel/drivers/md/dm-mod.$vtKoPo"
    echo $dmModPath > $VTOY_PATH/fsextract
    vtoy_unsquashfs -d $VTOY_PATH/sqfs -n -q -e $VTOY_PATH/fsextract $VTOY_PATH/fsdisk 2>>$VTLOG

    if ! [ -e $VTOY_PATH/sqfs${dmModPath} ]; then
        rm -rf $VTOY_PATH/sqfs
        dmModPath="/lib/modules/$vtKerVer/kernel/drivers/md/dm-mod.$vtKoPo"
        echo $dmModPath > $VTOY_PATH/fsextract
        vtoy_unsquashfs -d $VTOY_PATH/sqfs -n -q -e $VTOY_PATH/fsextract $VTOY_PATH/fsdisk 2>>$VTLOG
    fi
    
    if [ -e $VTOY_PATH/sqfs${dmModPath} ]; then
        vtlog "success $VTOY_PATH/sqfs${dmModPath}"
        insmod $VTOY_PATH/sqfs${dmModPath}
    else
        false
    fi
}


ventoy_os_install_dmsetup_by_fuse() {
    vtlog "ventoy_os_install_dmsetup_by_fuse $*"

    mkdir -p $VTOY_PATH/mnt/fuse $VTOY_PATH/mnt/iso $VTOY_PATH/mnt/squashfs

    vtoydm -p -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/ventoy_dm_table
    vtoy_fuse_iso -f $VTOY_PATH/ventoy_dm_table -m $VTOY_PATH/mnt/fuse

    mount -t iso9660  $VTOY_PATH/mnt/fuse/ventoy.iso    $VTOY_PATH/mnt/iso
    mount -t squashfs $VTOY_PATH/mnt/iso/antiX/linuxfs  $VTOY_PATH/mnt/squashfs

    KoName=$(ls $VTOY_PATH/mnt/squashfs/lib/modules/$2/kernel/drivers/md/dm-mod.ko*)
    vtlog "insmod $KoName"
    insmod $KoName

    umount $VTOY_PATH/mnt/squashfs
    umount $VTOY_PATH/mnt/iso
    umount $VTOY_PATH/mnt/fuse
}


ventoy_os_install_dmsetup() {
    vtlog "ventoy_os_install_dmsetup"
    
    if grep -q 'device-mapper' /proc/devices; then
        vtlog "device-mapper module already loaded"
        return;
    fi
    
    vtKerVer=$(uname -r)
    
    if ventoy_os_install_dmsetup_by_unsquashfs $1 $vtKerVer; then
        vtlog "unsquashfs success"
    else
        if modprobe fuse 2>>$VTLOG; then
            ventoy_os_install_dmsetup_by_fuse $1 $vtKerVer
        fi
    fi
}


wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

ventoy_os_install_dmsetup $vtdiskname

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

if ! [ -e $VTOY_DM_PATH ]; then
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    mknod -m 0666 $VTOY_DM_PATH b $blkdev_num
fi

PATH=$VTPATH_OLD

