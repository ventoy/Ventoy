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


ventoy_os_install_dmsetup_by_ko() {
    vtlog "ventoy_os_install_dmsetup_by_ko $1"
    
    vtVer=$(uname -r)
    if uname -m | $GREP -q 64; then
        vtBit=64
    else
        vtBit=32
    fi
    
    ventoy_extract_vtloopex $1  kerio
    vtLoopExDir=$VTOY_PATH/vtloopex/kerio/vtloopex
    
    if [ -e $vtLoopExDir/dm-mod/$vtVer/$vtBit/dm-mod.ko.xz ]; then
        $BUSYBOX_PATH/xz -d $vtLoopExDir/dm-mod/$vtVer/$vtBit/dm-mod.ko.xz
        insmod $vtLoopExDir/dm-mod/$vtVer/$vtBit/dm-mod.ko
    fi
}


wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

if echo $vtdiskname | $EGREP -q "nvme|mmc|nbd"; then
    ventoy_os_install_dmsetup_by_ko "${vtdiskname}p2"
else
    ventoy_os_install_dmsetup_by_ko "${vtdiskname}2"
fi


ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')
vtDM=$(ventoy_find_dm_id ${blkdev_num})

vtlog "/dev/$vtDM"
mount -t iso9660 /dev/$vtDM /cdrom
modprobe squashfs
echo "/dev/$vtDM" > /ventoy/vtDM

PATH=$VTPATH_OLD

set_ventoy_hook_finish
