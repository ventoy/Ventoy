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

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"
ventoy_create_dev_ventoy_part

if ventoy_need_dm_patch; then
    vtlog "extract a ko file"

    mkdir -p /ventoy/tmpmnt1 /ventoy/tmpmnt2
    mount /dev/ventoy1 /ventoy/tmpmnt1
    mount /ventoy/tmpmnt1/boot/batocera /ventoy/tmpmnt2
    vtKV=$(uname -r)
    
    mkdir -p /lib/modules/$vtKV/kernel/
    vtKO=$(find "/ventoy/tmpmnt2/lib/modules/$vtKV/kernel/fs/" -name "*.ko*" | head -n1)    
    cp -a $vtKO /lib/modules/$vtKV/kernel/
    
    vtlog "vtKV=$vtKV vtKO=$vtKO"
    
    umount /ventoy/tmpmnt2
    umount /ventoy/tmpmnt1

    vtPartid=1
    cat /vtoy_dm_table | while read vtline; do
        dmsetup remove ventoy$vtPartid
        vtPartid=$(expr $vtPartid + 1)
    done
    dmsetup remove ventoy
    
    vtlog "Recreate device-mapper"
    ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"
    ventoy_create_dev_ventoy_part
fi


PATH=$VTPATH_OLD

set_ventoy_hook_finish
