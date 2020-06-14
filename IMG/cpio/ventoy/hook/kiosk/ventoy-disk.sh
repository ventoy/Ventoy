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

ventoy_os_install_dmsetup_by_unsquashfs() {
    vtlog "ventoy_os_install_dmsetup_by_unsquashfs $*"
    
    vtKerVer=$(uname -r)
    vtKoPo=$(ventoy_get_module_postfix)
    vtlog "vtKerVer=$vtKerVer vtKoPo=$vtKoPo"

    vtoydm -i -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/iso_file_list

    vtline=$(grep '[-][-] .*kernel.xzm '  $VTOY_PATH/iso_file_list)    
    sector=$(echo $vtline | awk '{print $(NF-1)}')
    length=$(echo $vtline | awk '{print $NF}')
    
    vtlog "vtline=$vtline  sector=$sector  length=$length"
    
    vtoydm -e -f $VTOY_PATH/ventoy_image_map -d $1 -s $sector -l $length -o $VTOY_PATH/kernel.xzm
    mkdir -p $VTOY_PATH/sqfs  
    mount $VTOY_PATH/kernel.xzm  $VTOY_PATH/sqfs

    dmModPath="/lib/modules/$vtKerVer/kernel/drivers/md/dm-mod.$vtKoPo"    
    
    if [ -e $VTOY_PATH/sqfs${dmModPath} ]; then
        vtlog "success $VTOY_PATH/sqfs${dmModPath}"
        insmod $VTOY_PATH/sqfs${dmModPath}
    else
        vterr "failed $VTOY_PATH/sqfs${dmModPath}"
        false
    fi
    
    umount $VTOY_PATH/sqfs
    rm -f $VTOY_PATH/kernel.xzm
}

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

ventoy_os_install_dmsetup_by_unsquashfs $vtdiskname

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

PATH=$VTPATH_OLD
