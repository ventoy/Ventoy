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

if $GREP -q find_and_mount_installer /init; then
    echo "find_and_mount_installer" >> $VTLOG
    $SED "/find_and_mount_installer *$/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/clear/disk-hook.sh" -i  /init
else
    echo "find_installer" >> $VTLOG
    $SED "/\$.*find_installer/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/clear/disk-hook.sh" -i  /init
fi

#issue 1674
$SED "/switch_root/i $BUSYBOX_PATH/sh $VTOY_PATH/hook/clear/hidden-hook.sh" -i /init
