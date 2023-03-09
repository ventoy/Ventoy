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

vtKerVer=$(uname -r)
if uname -m | grep -q 64; then
    vtBit=64
else
    vtBit=32
fi

if [ -f $VTOY_PATH/vtloopex/dm-mod/$vtKerVer/$vtBit/dm-mod.ko.xz ]; then
    xz -d $VTOY_PATH/vtloopex/dm-mod/$vtKerVer/$vtBit/dm-mod.ko.xz
    insmod $VTOY_PATH/vtloopex/dm-mod/$vtKerVer/$vtBit/dm-mod.ko
elif [ -f $VTOY_PATH/modules/dm-mod.ko ]; then
    insmod $VTOY_PATH/modules/dm-mod.ko
fi

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

ventoy_create_dev_ventoy_part

mkdir /ventoy/mnt
mount /dev/ventoy2 /ventoy/mnt
rm -f /ventoy/mnt/.please_resize_me
sync
umount /ventoy/mnt

PATH=$VTPATH_OLD

set_ventoy_hook_finish
