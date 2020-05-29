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

. $VTOY_PATH/hook/ventoy-os-lib.sh

if [ -d /etc/udev/rules.d ] || [ -d /lib/udev/rules.d ]; then    
    ventoy_systemd_udevd_work_around
    ventoy_add_udev_rule "$VTOY_PATH/hook/default/udev_disk_hook.sh %k noreplace"
else
    if $GREP -q kaspersky /proc/version; then
        $SED "/sysresccd_stage1_normal[^(]*$/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/gentoo/disk_hook.sh"  -i /init
    else
        $SED "/mdev *-s/a\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/gentoo/disk_hook.sh"  -i /init
    fi
fi
