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

# TinyCore linux distro doesn't contain dmsetup, we use aoe here
sudo $BUSYBOX_PATH/modprobe aoe aoe_iflist=lo
if [ -e /sys/module/aoe ]; then
    VBLADE_BIN=$(ventoy_get_vblade_bin)
    sudo $VBLADE_BIN -r -f $VTOY_PATH/ventoy_image_map 9 0 lo "/dev/${1:0:-1}" &

    while ! [ -b /dev/etherd/e9.0 ]; do
        vtlog 'Wait for /dev/etherd/e9.0 ....'
        $SLEEP 0.1
    done

    sudo $BUSYBOX_PATH/cp -a /dev/etherd/e9.0  "/dev/$1"

    ventoy_find_bin_run rebuildfstab
else
    vterr "aoe driver module load failed..."
fi

# OK finish
set_ventoy_hook_finish
