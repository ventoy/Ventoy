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
    LINE=$($GREP 'device-mapper-[0-9]\..*\.rpm'  $VTOY_PATH/iso_file_list)
    if [ $? -eq 0 ]; then
        install_rpm_from_line "$LINE" ${vt_usb_disk}
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

if is_ventoy_hook_finished || not_ventoy_disk "${1:0:-1}"; then
    exit 0
fi

dmsetup_path=$(ventoy_find_bin_path dmsetup)
if [ -z "$dmsetup_path" ]; then
    ventoy_os_install_dmsetup "/dev/${1:0:-1}"
fi

if [ -f /proc/devices ]; then
    vtlog "/proc/devices exist OK"
else
    for i in 1 2 3 4 5 6 7 8 9; do
        if [ -f /proc/devices ]; then
            vtlog "/proc/devices exist OK now"
            break
        else
            vtlog "/proc/devices NOT exist, wait $i"
            $BUSYBOX_PATH/sleep 1
        fi
    done
fi


ventoy_udev_disk_common_hook $*

# OK finish
set_ventoy_hook_finish
