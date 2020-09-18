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

$SED 's#/dev/cdrom#/dev/ventoy#' -i /installer/isoInstaller.py

ventoy_set_inotify_script  photon/ventoy-inotifyd-hook.sh

[ -d /dev ] || $BUSYBOX_PATH/mkdir /dev
[ -d /sys ] || $BUSYBOX_PATH/mkdir /sys

$BUSYBOX_PATH/mount -t devtmpfs -o mode=0755,noexec,nosuid,strictatime devtmpfs /dev
$BUSYBOX_PATH/mount -t sysfs sys /sys

$BUSYBOX_PATH/rm -f /init
$BUSYBOX_PATH/sh $VTOY_PATH/hook/photon/ventoy-inotifyd-start.sh

