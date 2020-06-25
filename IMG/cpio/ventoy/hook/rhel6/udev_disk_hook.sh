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

vtCheatLoop=loop6

ventoy_os_install_dmsetup() {
    vtlog "ventoy_os_install_dmsetup $1"
    
    vt_usb_disk=$1

    $BUSYBOX_PATH/modprobe dm-mod
    $BUSYBOX_PATH/modprobe linear
    
    # dump iso file location
    $VTOY_PATH/tool/vtoydm -i -f $VTOY_PATH/ventoy_image_map -d ${vt_usb_disk} > $VTOY_PATH/iso_file_list

    # install dmsetup 
    LINE=$($GREP 'device-mapper-[0-9].*\.rpm'  $VTOY_PATH/iso_file_list)
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
    # /dev/vtCheatLoop come first
    if [ "$1" = "$vtCheatLoop" ] && [ -b $VTOY_DM_PATH ]; then
        ventoy_copy_device_mapper  /dev/$vtCheatLoop
    fi
    exit 0
fi

dmsetup_path=$(ventoy_find_bin_path dmsetup)
if [ -z "$dmsetup_path" ]; then
    ventoy_os_install_dmsetup "/dev/${1:0:-1}"
fi


#some distro add there repo file to /etc/anaconda.repos.d/ which will cause error during installation
#$BUSYBOX_PATH/nohup $VTOY_PATH/tool/inotifyd $VTOY_PATH/hook/rhel6/anaconda-repo-listen.sh /etc/anaconda.repos.d:n &  

ventoy_udev_disk_common_hook $* "noreplace"

$BUSYBOX_PATH/mount $VTOY_DM_PATH /mnt/ventoy

# 
# We do a trick for rhel6 series here.
# Use /dev/$vtCheatLoop and wapper it as a removable cdrom with bind mount.
# Then the anaconda installer will accept /dev/$vtCheatLoop as the install medium.
#
ventoy_copy_device_mapper  /dev/$vtCheatLoop

$BUSYBOX_PATH/cp -a /sys/devices/virtual/block/$vtCheatLoop /tmp/ >> $VTLOG 2>&1
echo 19 > /tmp/$vtCheatLoop/capability
$BUSYBOX_PATH/mount --bind /tmp/$vtCheatLoop /sys/block/$vtCheatLoop >> $VTLOG 2>&1

# OK finish
set_ventoy_hook_finish
