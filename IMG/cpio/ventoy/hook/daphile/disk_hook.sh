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

/sbin/mdev -s

# Just for KVM test environment
$BUSYBOX_PATH/modprobe virtio_blk 2>/dev/null
$BUSYBOX_PATH/modprobe virtio_pci 2>/dev/null

for i in 0 1 2 3 4 5 6 7 8 9; do 
    vtdiskname=$(get_ventoy_disk_name)
    if [ "$vtdiskname" = "unknown" ]; then
        vtlog "wait for disk ..."
        $SLEEP 2
    else
        break
    fi
done

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

$BUSYBOX_PATH/rm -f /dev/dm-*

