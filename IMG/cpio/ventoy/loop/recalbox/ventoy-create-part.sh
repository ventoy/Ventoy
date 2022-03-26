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

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

if [ -f /vtoy_dm_table ]; then
    vtPartCnt=$(cat /vtoy_dm_table | wc -l)
    if [ $vtPartCnt -ne 1 ]; then
        exit 0
    fi
else
    exit 0
fi

vtlog "try patch init script"

if [ -f /new_root/etc/init.d/S11share ]; then
    cp -a /new_root/etc/init.d/S11share /new_root/overlay/S11share
    sed "/^ *createMissingPartitions *$/r $VTOY_PATH/loop/recalbox/ventoy-share.sh" -i /new_root/overlay/S11share
    
    vtFile=$(ls -1 /new_root/etc/init.d/ | grep -m1 S01)
    
    mount --bind /new_root/overlay/S11share  /new_root/etc/init.d/$vtFile
    vtlog "patch S11share to $vtFile"
fi

PATH=$VTPATH_OLD
