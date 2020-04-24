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

drop_initramfs_workaround() {
    mainfilelist=$($FIND / -name 9990-main.sh)
    
    echo "mainfilelist=$mainfilelist" >> $VTLOG
    
    if [ -z "$mainfilelist" ]; then 
        return
    fi

    for vtfile in $mainfilelist; do
        vtcnt=$($GREP -c 'panic.*Unable to find a medium' $vtfile)
        if [ $vtcnt -ne 1 ]; then
            return
        fi
    done
    
    echo "direct_hook insert ..." >> $VTLOG
    
    for vtfile in $mainfilelist; do
        $SED "s#panic.*Unable to find a medium.*#$BUSYBOX_PATH/sh  $VTOY_PATH/hook/debian/deepin-disk.sh \$mountpoint; livefs_root=\$mountpoint#" -i $vtfile
    done
}

ventoy_systemd_udevd_work_around
ventoy_add_udev_rule "$VTOY_PATH/hook/debian/udev_disk_hook.sh %k"

drop_initramfs_workaround

