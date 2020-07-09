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

if $GREP -q 'Searching *for *PMAGIC' /init; then
    echo "Find Searching PMAGIC" >> $VTLOG
    $SED "/Searching *for *PMAGIC/a\ root=/dev/ventoy" -i  /init
    $SED "/Searching *for *PMAGIC/a\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/pmagic/disk-hook.sh" -i  /init
else
    echo "Use default..." >> $VTLOG    
    $SED "s#^root=.*cmdline.*#root=/dev/ventoy#g'" -i  /init
    ventoy_systemd_udevd_work_around
    ventoy_add_udev_rule "$VTOY_PATH/hook/pmagic/udev_disk_hook.sh %k"
fi
