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

    vtlog "ventoy_os_install_dmsetup $1 ..."

    vt_usb_disk=$1

    # dump iso file location
    $VTOY_PATH/tool/vtoydm -i -f $VTOY_PATH/ventoy_image_map -d ${vt_usb_disk} > $VTOY_PATH/iso_file_list

    # install dmsetup 
    LINE=$($GREP ' dmsetup.*\.udeb'  $VTOY_PATH/iso_file_list)
    if [ $? -eq 0 ]; then
        install_udeb_from_line "$LINE" ${vt_usb_disk}
    fi

    # install libdevmapper
    LINE=$($GREP ' libdevmapper.*\.udeb'  $VTOY_PATH/iso_file_list)
    if [ $? -eq 0 ]; then
        install_udeb_from_line "$LINE" ${vt_usb_disk}
    fi

    # install md-modules
    LINE=$($GREP -i ' md-modules.*\.udeb'  $VTOY_PATH/iso_file_list)
    if [ $? -eq 0 ]; then
        LINTCNT=$($GREP -i -c ' md-modules.*\.udeb'  $VTOY_PATH/iso_file_list)
        if [ $LINTCNT -gt 1 ]; then
            vtlog "more than one pkgs, need to filter..."
            VER=$($BUSYBOX_PATH/uname -r)
            
            LINE=$($GREP -i ' md-modules.*\.udeb'  $VTOY_PATH/iso_file_list | $GREP -i $VER)
            LINTCNT=$($GREP -i ' md-modules.*\.udeb'  $VTOY_PATH/iso_file_list | $GREP -i -c $VER)
            if [ $LINTCNT -gt 1 ]; then
                vtlog "Still more than one pkgs, use the first one..."
                LINE=$($GREP -i ' md-modules.*\.udeb'  $VTOY_PATH/iso_file_list | $GREP -i -m1 $VER)
            fi
        fi
        install_udeb_from_line "$LINE" ${vt_usb_disk} 
    fi

    # insmod md-mod if needed
    if $GREP -q 'device-mapper' /proc/devices; then
        vtlog "device mapper module is loaded"
    else
        vtlog "device mapper module is NOT loaded, now load it..."
        
        VER=$($BUSYBOX_PATH/uname -r)    
        KO=$($FIND /lib/modules/$VER/kernel/drivers/md -name "dm-mod*")
        vtlog "KO=$KO"
        
        insmod $KO
    fi
    
    vtlog "dmsetup install finish, now check it..."
    if dmsetup info >> $VTLOG 2>&1; then
        vtlog "dmsetup work ok"
    else
        vtlog "dmsetup not work, now try to load eglibc ..."
        
        # install eglibc (some ubuntu 32 bit version need it)
        LINE=$($GREP 'libc6-.*\.udeb'  $VTOY_PATH/iso_file_list)
        if [ $? -eq 0 ]; then
            install_udeb_from_line "$LINE" ${vt_usb_disk} 
        fi
        
        if dmsetup info >> $VTLOG 2>&1; then
            vtlog "dmsetup work ok after retry"
        else
            vtlog "dmsetup still not work after retry"
        fi
    fi
}

if is_ventoy_hook_finished || not_ventoy_disk "${1:0:-1}"; then
    exit 0
fi

vtlog "==== $0 $* ====" 

dmsetup_path=$(ventoy_find_bin_path dmsetup)
if [ -z "$dmsetup_path" ]; then
    ventoy_os_install_dmsetup "/dev/${1:0:-1}"
fi

if ! $GREP -q 'device-mapper' /proc/devices; then
    ventoy_os_install_dmsetup "/dev/${1:0:-1}"
fi

ventoy_udev_disk_common_hook $*

#
# Some distro default only accept usb partitions as install medium.
# So if ventoy is installed on a non-USB device, we just mount /cdrom here except
# for these has boot=live or boot=casper parameter in cmdline
#
VT_BUS_USB=""
if [ -n "$ID_BUS" ]; then
    if echo $ID_BUS | $GREP -q -i usb; then
        VT_BUS_USB="YES"
    fi
else
    if $BUSYBOX_PATH/ls -l /sys/class/block/${1:0:-1} | $GREP -q -i usb; then
        VT_BUS_USB="YES"
    fi
fi

if [ -n "$VT_BUS_USB" ]; then
    vtlog "$1 is USB device"
    echo /dev/$1 > /ventoy/list-devices-usb-part
else
    vtlog "$1 is NOT USB device (bus $ID_BUS)"
    
    if $EGREP -q 'boot=|casper' /proc/cmdline; then
        vtlog "boot=, or casper, don't mount"
    else
        vtlog "No boot param, need to mount"
        echo /dev/$1 > /ventoy/list-devices-usb-part
    fi
fi

#special process for Linx
if $BUSYBOX_PATH/uname -r | $GREP -q "^2\.6"; then
    if $GREP -q "linx" /proc/version; then
        blkdev_num=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')  
        vtDM=$(ventoy_find_dm_id ${blkdev_num}) 
        echo "/dev/$vtDM" > /ventoy/list-devices-usb-part
    fi
fi

# OK finish
set_ventoy_hook_finish
