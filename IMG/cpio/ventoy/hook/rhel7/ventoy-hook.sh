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

#ventoy_systemd_udevd_work_around
#ventoy_add_udev_rule "$VTOY_PATH/hook/default/udev_disk_hook.sh %k noreplace"

if $GREP -q 'root=live' /proc/cmdline; then
    $SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE root=live:/dev/dm-0#" -i /lib/dracut-lib.sh
else
    $SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE inst.stage2=hd:/dev/dm-0#" -i /lib/dracut-lib.sh
fi

ventoy_set_inotify_script  rhel7/ventoy-inotifyd-hook.sh

$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/rhel7/ventoy-inotifyd-start.sh /lib/dracut/hooks/pre-udev/01-ventoy-inotifyd-start.sh

# suppress write protected mount warning
if [ -e /usr/sbin/anaconda-diskroot ]; then
    $SED  's/^mount $dev $repodir/mount -oro $dev $repodir/' -i /usr/sbin/anaconda-diskroot
fi

