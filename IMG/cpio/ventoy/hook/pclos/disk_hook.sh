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

ventoy_os_install_device_mapper_by_unsquashfs() {
    vtlog "ventoy_os_install_device_mapper_by_unsquashfs $*"
    
    vtKoExt=$(ventoy_get_module_postfix)
    vtlog "vtKoExt=$vtKoExt"

    vtoydm -i -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/iso_file_list

    vtline=$(grep '[-][-] livecd.sqfs '  $VTOY_PATH/iso_file_list)    
    sector=$(echo $vtline | awk '{print $(NF-1)}')
    length=$(echo $vtline | awk '{print $NF}')
    
    vtoydm -E -f $VTOY_PATH/ventoy_image_map -d $1 -s $sector -l $length -o $VTOY_PATH/fsdisk
    
    dmModPath="/lib/modules/$2/kernel/drivers/md/dm-mod.$vtKoExt"
    echo $dmModPath > $VTOY_PATH/fsextract
    vtoy_unsquashfs -d $VTOY_PATH/sqfs -n -q -e $VTOY_PATH/fsextract $VTOY_PATH/fsdisk

    if [ -e $VTOY_PATH/sqfs${dmModPath} ]; then
        vtlog "success $VTOY_PATH/sqfs${dmModPath}"
        insmod $VTOY_PATH/sqfs${dmModPath}
    else
        false
    fi
}


ventoy_os_install_device_mapper() {
    vtlog "ventoy_os_install_device_mapper"
    
    if grep -q 'device-mapper' /proc/devices; then
        vtlog "device-mapper module already loaded"
        return;
    fi
    
    vtKerVer=$(uname -r)
    if ventoy_os_install_device_mapper_by_unsquashfs $1 $vtKerVer; then
        vtlog "unsquashfs success"
    else
        vterr "unsquashfs failed"
    fi
}

vtdiskname=$(get_ventoy_disk_name)
ventoy_os_install_device_mapper $vtdiskname

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2"

PATH=$VTPATH_OLD
