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

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

vtlog "${vtdiskname#/dev/}2 found..."
$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/udev_disk_hook.sh "${vtdiskname#/dev/}2"

if [ -f /ventoy/autoinstall ]; then
    sh /ventoy/hook/default/auto_install_varexp.sh  /ventoy/autoinstall
fi
