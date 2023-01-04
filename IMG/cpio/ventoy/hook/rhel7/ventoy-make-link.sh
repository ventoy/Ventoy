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

if [ ! -e /dev/dm-0 ]; then
    exit 0
fi

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')  
vtDM=$(ventoy_find_dm_id ${blkdev_num})

if [ -e /dev/mapper/ventoy ]; then
    vtlog "/dev/mapper/ventoy already exist"
else
    vtlog "link /dev/$vtDM to /dev/mapper/ventoy"
    ln -s /dev/$vtDM /dev/mapper/ventoy
fi

VTLABEL=$($BUSYBOX_PATH/blkid /dev/$vtDM | $SED 's/.*LABEL="\([^"]*\)".*/\1/')
vtlog "VTLABEL=$VTLABEL"

if [ -n "$VTLABEL" ]; then
    if ! [ -d /dev/disk/by-label ]; then
        mkdir -p /dev/disk/by-label
    fi

    if [ -e "/dev/disk/by-label/$VTLABEL" ]; then
        vtlog "/dev/disk/by-label/$VTLABEL already exist"
    else
        vtlog "link /dev/$vtDM to /dev/disk/by-label/$VTLABEL"
        ln -s /dev/$vtDM "/dev/disk/by-label/$VTLABEL"
    fi
fi
