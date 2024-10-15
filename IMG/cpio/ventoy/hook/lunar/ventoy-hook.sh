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

$SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE root=/dev/ventoy#" -i /lib/dracut-lib.sh

$BUSYBOX_PATH/rm -f /usr/lib/systemd/system-generators/systemd-fstab-generator /lib/systemd/system-generators/systemd-fstab-generator

ventoy_set_inotify_script  lunar/ventoy-inotifyd-hook.sh
$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/lunar/ventoy-inotifyd-start.sh /lib/dracut/hooks/pre-udev/01-ventoy-inotifyd-start.sh
