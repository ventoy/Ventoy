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

$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/default/10-dm.rules        /lib/udev/rules.d/
$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/default/13-dm-disk.rules   /lib/udev/rules.d/

$SED "/^\"\$mount_handler\"/i\ $BUSYBOX_PATH/sh $VTOY_PATH/loop/ubos/ventoy-disk.sh" -i /init

$SED "/^\"\$mount_handler\"/a\ $BUSYBOX_PATH/sh $VTOY_PATH/loop/ubos/newroot-hook.sh" -i /init

$SED "/^\"\$mount_handler\"/a\ init=/usr/lib/systemd/systemd" -i /init

