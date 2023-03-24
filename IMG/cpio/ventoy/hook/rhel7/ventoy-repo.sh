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

vtlog "##### $0 $* ..."

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

if [ -f /ventoy/vtoy_iso_scan ]; then
    repopath=$(cat /ventoy/vtoy_iso_scan)
    repodev=$(vtoydump -f /ventoy/ventoy_os_param | awk -F'#' '{print $1}')
    if echo $repodev | egrep -q "nvme|mmc|nbd"; then
        vtpart1=${repodev}p1
    else
        vtpart1=${repodev}1
    fi
    echo "inst.repo=hd:${vtpart1}:${repopath}" >> /sysroot/etc/cmdline
else
    repodev=$(ls $VTOY_PATH/dev_backup*)
    echo "inst.repo=hd:/dev/${repodev#*dev_backup_}" >> /sysroot/etc/cmdline
fi

PATH=$VTPATH_OLD
