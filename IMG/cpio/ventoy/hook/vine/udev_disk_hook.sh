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

ventoy_os_install_dmsetup_by_ko() {
    vtlog "ventoy_os_install_dmsetup_by_ko $1"
    
    vtVer=$($BUSYBOX_PATH/uname -r)
    if $BUSYBOX_PATH/uname -m | $GREP -q 64; then
        vtBit=64
    else
        vtBit=32
    fi
    
    ventoy_extract_vtloopex $1  vine
    vtLoopExDir=$VTOY_PATH/vtloopex/vine/vtloopex
    
    if [ -e $vtLoopExDir/dm-mod/$vtVer/$vtBit/dm-mod.ko.xz ]; then
        $BUSYBOX_PATH/xz -d $vtLoopExDir/dm-mod/$vtVer/$vtBit/dm-mod.ko.xz
        insmod $vtLoopExDir/dm-mod/$vtVer/$vtBit/dm-mod.ko
    fi
}

ventoy_os_install_dmsetup_by_rpm() {
    vtlog "ventoy_os_install_dmsetup_by_rpm $1"
    
    vt_usb_disk=$1

    # dump iso file location
    $VTOY_PATH/tool/vtoydm -i -f $VTOY_PATH/ventoy_image_map -d ${vt_usb_disk} > $VTOY_PATH/iso_file_list

    # install dmsetup 
    LINE=$($GREP 'kernel-[0-9].*\.rpm'  $VTOY_PATH/iso_file_list)
    if [ $? -eq 0 ]; then
        extract_rpm_from_line "$LINE" ${vt_usb_disk}
    fi

    vtKoName=$($BUSYBOX_PATH/find $VTOY_PATH/rpm/ -name dm-mod.ko*)
    vtlog "vtKoName=$vtKoName"

    insmod $vtKoName
    
    $BUSYBOX_PATH/rm -rf $VTOY_PATH/rpm/
}

if is_ventoy_hook_finished || not_ventoy_disk "${1:0:-1}"; then    
    exit 0
fi

ventoy_os_install_dmsetup_by_ko "/dev/$1"
if $GREP -q 'device.mapper' /proc/devices; then
    vtlog "device-mapper module install OK"
else
    ventoy_os_install_dmsetup_by_rpm "/dev/${1:0:-1}"
fi

ventoy_udev_disk_common_hook $*

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')
blkdev_num_dev=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
vtDM=$(ventoy_find_dm_id ${blkdev_num})

[ -b /dev/$vtDM ] || $BUSYBOX_PATH/mknod -m 0660 /dev/$vtDM b $blkdev_num_dev
$BUSYBOX_PATH/rm -rf /dev/mapper

#Create a fake IDE-CDROM driver can't be ide-scsi   /proc has been patched to /vtoy
$BUSYBOX_PATH/mkdir -p /vtoy
$BUSYBOX_PATH/cp -a /proc/ide /vtoy/
$BUSYBOX_PATH/mkdir -p /vtoy/ide/aztcd

echo 'ide' > /vtoy/ide/aztcd/driver
echo 'cdrom' > /vtoy/ide/aztcd/media


# OK finish
set_ventoy_hook_finish

exit 0
