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

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH


ventoy_os_install_dmsetup_by_fuse() {
    vtlog "ventoy_os_install_dmsetup_by_fuse $*"

    mkdir -p $VTOY_PATH/mnt/fuse $VTOY_PATH/mnt/iso $VTOY_PATH/mnt/squashfs

    vtoydm -p -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/ventoy_dm_table
    vtoy_fuse_iso -f $VTOY_PATH/ventoy_dm_table -m $VTOY_PATH/mnt/fuse

    mount -t iso9660  $VTOY_PATH/mnt/fuse/ventoy.iso    $VTOY_PATH/mnt/iso
    
    [ -f $VTOY_PATH/mnt/iso/system/squashfs_sys.img ] && mount -t squashfs $VTOY_PATH/mnt/iso/system/squashfs_sys.img  $VTOY_PATH/mnt/squashfs
    [ -f $VTOY_PATH/mnt/iso/system/squashfs.img ] && mount -t squashfs $VTOY_PATH/mnt/iso/system/squashfs.img  $VTOY_PATH/mnt/squashfs

    KoName=$(ls $VTOY_PATH/mnt/squashfs/lib/modules/$2/kernel/drivers/md/dm-mod.ko*)
    vtlog "insmod $KoName"
    insmod $KoName

    umount $VTOY_PATH/mnt/squashfs   2>>$VTLOG
    umount $VTOY_PATH/mnt/iso        2>>$VTLOG
    umount $VTOY_PATH/mnt/fuse       2>>$VTLOG
}


wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

modprobe fuse
ventoy_os_install_dmsetup_by_fuse $vtdiskname $(uname -r)

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
mknod -m 0660 /dev/ventoy b $blkdev_num

PATH=$VTPATH_OLD
