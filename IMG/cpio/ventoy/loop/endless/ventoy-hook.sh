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

END_UDEV_DIR=$(ventoy_get_udev_conf_dir)

if ! [ -e "$END_UDEV_DIR/10-dm.rules" ]; then
    echo 'Copy dm rule file' >> $VTLOG
    $CAT $VTOY_PATH/hook/default/10-dm.rules > "$END_UDEV_DIR/10-dm.rules"
fi

if ! [ -e "$END_UDEV_DIR/13-dm-disk.rules" ]; then
    echo 'Copy dm-disk rule file' >> $VTLOG
    $CAT $VTOY_PATH/hook/default/13-dm-disk.rules > "$END_UDEV_DIR/13-dm-disk.rules"
fi

ventoy_set_loop_inotify_script  endless/ventoy-inotifyd-hook.sh
$BUSYBOX_PATH/cp -a $VTOY_PATH/loop/endless/ventoy-inotifyd-start.sh /lib/dracut/hooks/pre-udev/01-ventoy-inotifyd-start.sh

