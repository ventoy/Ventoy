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

if is_ventoy_hook_finished || not_ventoy_disk "${1:0:-1}"; then
    exit 0
fi

ventoy_udev_disk_common_hook $*

#
# cheatcode for mageia
#
# From mageia/soft/drakx/mdk-stage1 source code, we see that the stage1 binary will search 
# /tmp/syslog file to determin whether there is a DAC960 cdrom in the system.
# So we insert some string to /tmp/syslog file to cheat the stage1 program.
#
$BUSYBOX_PATH/mkdir -p /dev/rd
ventoy_copy_device_mapper "/dev/rd/ventoy"
echo 'ventoy cheatcode /dev/rd/ventoy:  model' >> /tmp/syslog

# OK finish
set_ventoy_hook_finish
