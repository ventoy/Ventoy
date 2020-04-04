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

ventoy_os_install_dmsetup() {
    vtlog "ventoy_os_install_dmsetup $1"
    
    vt_usb_disk=$1
    
    # dump iso file location
    $VTOY_PATH/tool/vtoydm -i -f $VTOY_PATH/ventoy_image_map -d ${vt_usb_disk} > $VTOY_PATH/iso_file_list

    # install dmsetup 
    LINE=$($GREP 'minstg2.img'  $VTOY_PATH/iso_file_list)
    if [ $? -eq 0 ]; then
        extract_file_from_line "$LINE" ${vt_usb_disk} /tmp/minstg2.img
        
        mkdir -p /tmp/ramfs/minstg2.img
        mount -t squashfs  /tmp/minstg2.img   /tmp/ramfs/minstg2.img
        
        $BUSYBOX_PATH/ln -s /tmp/ramfs/minstg2.img/lib64 /lib64
        $BUSYBOX_PATH/ln -s /tmp/ramfs/minstg2.img/usr /usr
    fi

    vtlog "dmsetup install finish, now check it..."
    
    dmsetup_path=$(ventoy_find_bin_path dmsetup)
    if [ -z "$dmsetup_path" ]; then
        vterr "dmsetup still not found after install"
    elif $dmsetup_path info >> $VTLOG 2>&1; then
        vtlog "$dmsetup_path work ok"
    else
        vterr "$dmsetup_path not work"
    fi
}

vtlog "##### $0 $* ########"

vtdiskname=$(get_ventoy_disk_name)

dmsetup_path=$(ventoy_find_bin_path dmsetup)
if [ -z "$dmsetup_path" ]; then
    ventoy_os_install_dmsetup "$vtdiskname"
    ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"
    
    $BUSYBOX_PATH/unlink /lib64
    $BUSYBOX_PATH/unlink /usr
    umount /tmp/ramfs/minstg2.img
    rm -rf /tmp/ramfs/minstg2.img
    rm -f /tmp/minstg2.img 
else
    ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"
fi
