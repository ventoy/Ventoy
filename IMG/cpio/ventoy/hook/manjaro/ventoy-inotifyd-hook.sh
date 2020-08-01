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

if is_inotify_ventoy_part $3; then
    vtlog "##### INOTIFYD: $2/$3 is created (YES) ..."
    vtlog "find ventoy partition $3 ..."    
    $BUSYBOX_PATH/sh $VTOY_PATH/hook/default/udev_disk_hook.sh "$3"
    
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')  
    vtDM=$(ventoy_find_dm_id ${blkdev_num})
    vtLABEL=$($BUSYBOX_PATH/blkid /dev/$vtDM | $AWK '{print $2}' | $SED 's/.*"\(.*\)".*/\1/')

    vtlog "blkdev_num=$blkdev_num  vtDM=$vtDM  label $vtLABEL ..."
    
    if ! [ -d /dev/disk/by-label ]; then
        mkdir -p /dev/disk/by-label
    fi
    $BUSYBOX_PATH/cp -a /dev/$vtDM /dev/disk/by-label/$vtLABEL
else
    vtlog "##### INOTIFYD: $2/$3 is created (NO) ..."
fi

PATH=$VTPATH_OLD
