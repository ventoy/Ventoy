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

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

vtlog "Loading dax and dm-mod module ..."
$BUSYBOX_PATH/modprobe dax      > /dev/null 2>&1
$BUSYBOX_PATH/modprobe dm-mod   > /dev/null 2>&1

if $GREP -q 'device-mapper' /proc/devices; then
    vtlog "dm-mod module check success ..."
else
    vtlog "Need to extract dax and dm-mod module ..."
    $VTOY_PATH/tool/zstdcat /lib/modules/$(uname -r)/drivers/dax/dax.ko.zst > $VTOY_PATH/extract_dax.ko
    $BUSYBOX_PATH/insmod $VTOY_PATH/extract_dax.ko
    $VTOY_PATH/tool/zstdcat /lib/modules/$(uname -r)/drivers/md/dm-mod.ko.zst > $VTOY_PATH/extract_dm_mod.ko
    $BUSYBOX_PATH/insmod $VTOY_PATH/extract_dm_mod.ko
fi

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
mknod -m 0660 /dev/ventoy b $blkdev_num

PATH=$VTPATH_OLD

set_ventoy_hook_finish

