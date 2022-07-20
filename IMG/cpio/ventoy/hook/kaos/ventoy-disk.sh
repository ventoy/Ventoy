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

vtlog "######### $0 $* ############"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH


ventoy_os_install_dmsetup_by_fuse() {
    vtlog "ventoy_os_install_dmsetup_by_fuse $*"

    mkdir -p $VTOY_PATH/mnt/fuse $VTOY_PATH/mnt/iso $VTOY_PATH/mnt/squashfs

    vtoydm -p -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/ventoy_dm_table
    vtoy_fuse_iso -f $VTOY_PATH/ventoy_dm_table -m $VTOY_PATH/mnt/fuse

    mount -t iso9660  $VTOY_PATH/mnt/fuse/ventoy.iso    $VTOY_PATH/mnt/iso

    sfsfile=$VTOY_PATH/mnt/iso/kdeos/x86_64/root-image.sqfs

    mount -t squashfs $sfsfile  $VTOY_PATH/mnt/squashfs

    kVer=$(uname -r)
    KoName=$(ls $VTOY_PATH/mnt/squashfs/lib/modules/$kVer/kernel/drivers/md/dm-mod.ko*)
    vtlog "insmod $KoName"

    if echo $KoName | grep -q '\.zst'; then
        zstdcat $KoName > $VTOY_PATH/dm-mod.ko
        insmod $VTOY_PATH/dm-mod.ko
    else
        insmod $KoName
    fi
    
    ventoy_check_umount $VTOY_PATH/mnt/squashfs
    ventoy_check_umount $VTOY_PATH/mnt/iso
    ventoy_check_umount $VTOY_PATH/mnt/fuse
    vtlog "umount done"
}


if is_ventoy_hook_finished; then
    exit 0
fi

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    exit 0
fi

if grep -q 'device-mapper' /proc/devices; then
    vtlog "device-mapper module exist"
else
    ventoy_os_install_dmsetup_by_fuse  $vtdiskname
fi

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')
vtDM=$(ventoy_find_dm_id ${blkdev_num})
vtlog "blkdev_num=$blkdev_num vtDM=$vtDM ..."

while [ -n "Y" ]; do
    if [ -b /dev/$vtDM ]; then
        break
    else
        sleep 0.3
    fi
done

if [ -n "$1" ]; then
    vtlog "ln -s /dev/$vtDM $1"
    
    if [ -e "$1" ]; then
        vtlog "$1 already exist"
    else
        ln -s /dev/$vtDM "$1"
    fi
else
    vtLABEL=$($BUSYBOX_PATH/blkid /dev/$vtDM | $SED 's/.*LABEL="\([^"]*\)".*/\1/')
    vtlog "vtLABEL is $vtLABEL"
    
    if [ -z "$vtLABEL" ]; then
        vtLABEL=$($SED "s/.*label=\([^ ]*\)/\1/" /proc/cmdline)
        vtlog "vtLABEL is $vtLABEL from cmdline"
    fi
    
    if [ -e "/dev/disk/by-label/$vtLABEL" ]; then
        vtlog "$1 already exist"
    else
        ln -s /dev/$vtDM "/dev/disk/by-label/$vtLABEL"
    fi
fi 

PATH=$VTPATH_OLD

# OK finish
set_ventoy_hook_finish
