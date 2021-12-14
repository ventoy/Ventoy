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
    mount -t squashfs $VTOY_PATH/mnt/iso/pyabr/01-core.sb  $VTOY_PATH/mnt/squashfs

    KoName=$(ls $VTOY_PATH/mnt/squashfs/lib/modules/$2/kernel/drivers/md/dm-mod.ko*)
    vtlog "insmod $KoName"
    insmod $KoName

    umount $VTOY_PATH/mnt/squashfs
    umount $VTOY_PATH/mnt/iso
    umount $VTOY_PATH/mnt/fuse
}

while [ -n "Y" ]; do
    vtdiskname=$(get_ventoy_disk_name)
    if [ "$vtdiskname" = "unknown" ]; then
        vtlog "ventoy disk not found"
        if [ -r /proc/sys/kernel/hotplug ]; then
            echo /sbin/mdev > /proc/sys/kernel/hotplug
        fi
        mdev -s
        sleep 1
    else
        if check_usb_disk_ready "$vtdiskname"; then
            vtlog "check_usb_disk_ready $vtdiskname ok"
            break
        else
            vtlog "check_usb_disk_ready $vtdiskname error"
            if [ -r /proc/sys/kernel/hotplug ]; then
                echo /sbin/mdev > /proc/sys/kernel/hotplug
            fi
            mdev -s
            sleep 1
        fi
    fi
done

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

modprobe fuse
modprobe cuse

vtKver=$(uname -r)
ventoy_os_install_dmsetup_by_fuse $vtdiskname $vtKver

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

PATH=$VTPATH_OLD

set_ventoy_hook_finish
