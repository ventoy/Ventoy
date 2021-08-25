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

vine_wait_for_exist() {
    while [ -n "1" ]; do
        if [ -e $1 ]; then
            break
        else
            $SLEEP 0.5
        fi
    done
}

vine_wait_for_exist /dev/null
vine_wait_for_exist /sys/block
vine_wait_for_exist /proc/ide


while [ -n "Y" ]; do
    vtdiskname=$(get_ventoy_disk_name)
    if [ "$vtdiskname" != "unknown" ]; then
        break
    else
        $SLEEP 0.5
    fi
done

vtshortdev=${vtdiskname#/dev/}

if ! [ -b $vtdiskname ]; then
    blkdev=$($CAT /sys/class/block/$vtshortdev/dev | $SED 's/:/ /g')
    $BUSYBOX_PATH/mknod -m 0660 $vtdiskname b $blkdev
fi

if ! [ -b "${vtdiskname}1" ]; then
    blkdev=$($CAT /sys/class/block/${vtshortdev}1/dev | $SED 's/:/ /g')
    $BUSYBOX_PATH/mknod -m 0660 "${vtdiskname}1" b $blkdev
fi

if ! [ -b "${vtdiskname}2" ]; then
    blkdev=$($CAT /sys/class/block/${vtshortdev}2/dev | $SED 's/:/ /g')
    $BUSYBOX_PATH/mknod -m 0660 "${vtdiskname}2" b $blkdev
fi

$BUSYBOX_PATH/sh $VTOY_PATH/hook/vine/udev_disk_hook.sh "${vtdiskname#/dev/}2"
