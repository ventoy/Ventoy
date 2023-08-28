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

if is_ventoy_hook_finished; then
    exit 0
fi

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

ventoy_os_install_dmsetup_by_fuse() {
    local drvdir=""
    vtlog "ventoy_os_install_dmsetup_by_fuse $*"

    mkdir -p $VTOY_PATH/mnt/fuse $VTOY_PATH/mnt/iso $VTOY_PATH/mnt/squashfs

    vtoydm -p -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/ventoy_dm_table
    vtoy_fuse_iso -f $VTOY_PATH/ventoy_dm_table -m $VTOY_PATH/mnt/fuse

    mount -t iso9660  $VTOY_PATH/mnt/fuse/ventoy.iso    $VTOY_PATH/mnt/iso
    
    
    for sfsfile in $(ls $VTOY_PATH/mnt/iso/*drv_veket*.sfs); do
        mount -t squashfs $sfsfile  $VTOY_PATH/mnt/squashfs        
        if [ -d $VTOY_PATH/mnt/squashfs/lib/modules ]; then
            KoName=$(ls $VTOY_PATH/mnt/squashfs/lib/modules/$2/kernel/drivers/md/dm-mod.ko*)
            if [ -n "$KoName" -a -f $KoName ]; then
                drvdir=$VTOY_PATH/mnt/squashfs/lib/modules/$2
                break
            fi
        fi
        
        umount $VTOY_PATH/mnt/squashfs
    done


    if [ -z "$drvdir" ]; then
        vtlog "retry for usr/lib dir"
        for sfsfile in $(ls $VTOY_PATH/mnt/iso/*drv_veket*.sfs); do
            mount -t squashfs $sfsfile  $VTOY_PATH/mnt/squashfs        
            if [ -d $VTOY_PATH/mnt/squashfs/usr/lib/modules ]; then
                KoName=$(ls $VTOY_PATH/mnt/squashfs/usr/lib/modules/$2/kernel/drivers/md/dm-mod.ko*)
                if [ -n "$KoName" -a -f $KoName ]; then
                    drvdir=$VTOY_PATH/mnt/squashfs/usr/lib/modules/$2
                    break
                fi
            fi
            
            umount $VTOY_PATH/mnt/squashfs
        done
    fi
    

    KoName=$(ls $drvdir/kernel/drivers/dax/dax.ko*)
    vtlog "insmod $KoName"
    insmod $KoName 
    
    KoName=$(ls $drvdir/kernel/drivers/md/dm-mod.ko*)
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
    
    ventoy_os_install_dmsetup_by_fuse $1 $vtKerVer
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

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
mknod -m 0666 /dev/ventoy b $blkdev_num

PATH=$VTPATH_OLD

set_ventoy_hook_finish
