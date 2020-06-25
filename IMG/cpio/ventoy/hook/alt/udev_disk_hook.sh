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

if is_ventoy_hook_finished || not_ventoy_disk "${1:0:-1}"; then
    exit 0
fi

ventoy_udev_disk_common_hook $* "noreplace"

if ! [ -e $VTOY_DM_PATH ]; then
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    mknod -m 0666 $VTOY_DM_PATH b $blkdev_num
fi

# 
# We do a trick for ATL series here.
# Use /dev/vtCheatLoop and wapper it as a cdrom with bind mount.
# Then the installer will accept /dev/vtCheatLoop as the install medium.
#
vtCheatLoop=loop6
ventoy_copy_device_mapper  /dev/$vtCheatLoop
$BUSYBOX_PATH/mkdir -p /tmp/$vtCheatLoop/device/
echo 5 > /tmp/$vtCheatLoop/device/type
$BUSYBOX_PATH/mount --bind /tmp/$vtCheatLoop /sys/block/$vtCheatLoop >> $VTLOG 2>&1


# OK finish
set_ventoy_hook_finish

