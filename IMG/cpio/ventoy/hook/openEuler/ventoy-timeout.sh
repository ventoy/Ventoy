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

if [ ! -e /dev/ventoy ]; then
    blkdev_num_mknod=$(dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    mknod -m 660 /dev/ventoy  b  $blkdev_num_mknod
fi

if [ -f $VTOY_PATH/ventoy_ks_rootpath ]; then
    vt_ks_rootpath=$(cat $VTOY_PATH/ventoy_ks_rootpath)
    vtlog "ks rootpath <$vt_ks_rootpath>"
    if [ -e /sbin/fetch-kickstart-disk ]; then
        vtlog "fetch-kickstart-disk ..."        
        /sbin/fetch-kickstart-disk /dev/ventoy "$vt_ks_rootpath"
    fi
fi

if [ -e /sbin/anaconda-diskroot ]; then
    vtlog "set anaconda-diskroot /dev/ventoy ..."

    #busybox cp doesn't support -t option (issue 1900)
    /bin/cp -a /bin/cp $BUSYBOX_PATH/cp
    /sbin/anaconda-diskroot /dev/ventoy
fi

PATH=$VTPATH_OLD
