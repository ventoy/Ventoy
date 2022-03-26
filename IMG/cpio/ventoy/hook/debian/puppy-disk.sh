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
    vtlog "ventoy_os_install_dmsetup_by_fuse $*"

    mkdir -p $VTOY_PATH/mnt/fuse $VTOY_PATH/mnt/iso $VTOY_PATH/mnt/squashfs

    vtoydm -p -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/ventoy_dm_table
    vtoy_fuse_iso -f $VTOY_PATH/ventoy_dm_table -m $VTOY_PATH/mnt/fuse

    mount -t iso9660  $VTOY_PATH/mnt/fuse/ventoy.iso    $VTOY_PATH/mnt/iso

    sfsfile=$(ls $VTOY_PATH/mnt/iso/*.sfs)

    mount -t squashfs $sfsfile  $VTOY_PATH/mnt/squashfs

    kVer=$(uname -r)
    KoName=$(ls $VTOY_PATH/mnt/squashfs/lib/modules/$kVer/kernel/drivers/md/dm-mod.ko*)
    vtlog "insmod $KoName"
    insmod $KoName

    umount $VTOY_PATH/mnt/squashfs
    umount $VTOY_PATH/mnt/iso
    umount $VTOY_PATH/mnt/fuse
}




wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

if grep -q 'device-mapper' /proc/devices; then
    vtlog "device-mapper module exist"
else
    ventoy_os_install_dmsetup_by_fuse  $vtdiskname
fi

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

if ! [ -e $VTOY_DM_PATH ]; then
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    mknod -m 0666 $VTOY_DM_PATH b $blkdev_num
fi
cp -a $VTOY_DM_PATH /dev/ventoy

PATH=$VTPATH_OLD

set_ventoy_hook_finish
