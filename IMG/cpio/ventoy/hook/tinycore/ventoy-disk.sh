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

if is_ventoy_hook_finished; then
    exit 0
fi

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

if echo $vtdiskname | egrep -q "nvme.*p[0-9]$|mmc.*p[0-9]$|nbd.*p[0-9]$"; then
    vPart="${vtdiskname}p2"    
else
    vPart="${vtdiskname}2"
fi

# TinyCore linux distro doesn't contain dmsetup, we use aoe here
sudo modprobe aoe aoe_iflist=lo
if [ -e /sys/module/aoe ]; then
    
    if ! [ -d /lib64 ]; then
        vtlog "link lib64"
        NEED_UNLIB64=1
        ln -s /lib /lib64
    fi
    
    VBLADE_BIN=$(ventoy_get_vblade_bin)

    sudo nohup $VBLADE_BIN -r -f $VTOY_PATH/ventoy_image_map 9 0 lo "$vtdiskname" > /dev/null & 
    sleep 2

    while ! [ -b /dev/etherd/e9.0 ]; do
        vtlog 'Wait for /dev/etherd/e9.0 ....'
        sleep 2
    done
    
    sudo cp -a /dev/etherd/e9.0  "$vPart"

    if [ -n "$NEED_UNLIB64" ]; then
        vtlog "unlink lib64"
        unlink /lib64
    fi

    ventoy_find_bin_run rebuildfstab
else
    vterr "aoe driver module load failed..."
fi

# OK finish
PATH=$VTPATH_OLD

set_ventoy_hook_finish
