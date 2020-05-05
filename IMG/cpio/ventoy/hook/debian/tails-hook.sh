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

$SED "s#.*livefs_root=.*find_livefs.*#$BUSYBOX_PATH/mount -t iso9660 /dev/mapper/ventoy \$mountpoint; livefs_root=\$mountpoint#" -i /usr/lib/live/boot/9990-main.sh
$SED "s#.*livefs_root=.*find_livefs.*#$BUSYBOX_PATH/mount -t iso9660 /dev/mapper/ventoy \$mountpoint; livefs_root=\$mountpoint#" -i /usr/bin/boot/9990-main.sh

ventoy_systemd_udevd_work_around
ventoy_add_udev_rule "$VTOY_PATH/hook/debian/udev_disk_hook.sh %k"