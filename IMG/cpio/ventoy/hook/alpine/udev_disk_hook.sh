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

if [ "$SUBSYSTEM" != "block" ] || [ "$DEVTYPE" != "partition" ]; then    
    exit 0
fi

if [ -b /dev/${MDEV:0:-1} ]; then
    vtlog "/dev/${MDEV:0:-1} exist"
else
    $SLEEP 2
fi

if is_ventoy_hook_finished || not_ventoy_disk "${MDEV:0:-1}"; then
    exit 0
fi

PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

#
# longpanda:
# Alpine initramfs doesn't contain dm-mod or fuse module, 
# and even the worse, the libpthread.so is not included too.
# So here we directly dump the modloop squashfs file from disk to rootfs.
# Fortunately, this file is not too big (< 100MB in alpine 3.11.3).
# After that:
#   1. mount the squashfs file
#   2. find the dm-mod module from the mountpoint and insmod
#   3. unmount and delete the squashfs file
#

vtoydm -i -f $VTOY_PATH/ventoy_image_map -d /dev/${MDEV:0:-1} > $VTOY_PATH/iso_file_list

vtLine=$(grep '[-][-] modloop-lts ' $VTOY_PATH/iso_file_list)
sector=$(echo $vtLine | awk '{print $(NF-1)}')
length=$(echo $vtLine | awk '{print $NF}')

vtoydm -e -f $VTOY_PATH/ventoy_image_map -d /dev/${MDEV:0:-1} -s $sector -l $length -o /vt_modloop

mkdir -p $VTOY_PATH/mnt
mount /vt_modloop $VTOY_PATH/mnt

KoModPath=$(find $VTOY_PATH/mnt/ -name 'dm-mod.ko*')
vtlog "KoModPath=$KoModPath"

if modinfo $KoModPath | grep -q 'depend.*dax'; then
    vtlog "First install dax mod ..."
    DaxModPath=$(echo $KoModPath | sed 's#md/dm-mod#dax/dax#')
    vtlog "insmod $DaxModPath"
    insmod $DaxModPath
fi

insmod $KoModPath

umount $VTOY_PATH/mnt
rm -f /vt_modloop

ventoy_udev_disk_common_hook "$MDEV" "noreplace"

# OK finish
set_ventoy_hook_finish
