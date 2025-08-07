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

if is_ventoy_hook_finished; then
    exit 0
fi

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

for i in 0 1 2 3 4 5 6 7 8 9; do
    vtdiskname=$(get_ventoy_disk_name)
    if [ "$vtdiskname" = "unknown" ]; then
        vtlog "wait for disk ..."
        sleep 3
    else
        break
    fi
done

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
mknod -m 0666 /dev/ventoy b $blkdev_num

set_ventoy_hook_finish

PATH=$VTPATH_OLD


