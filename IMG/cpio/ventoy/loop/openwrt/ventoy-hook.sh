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

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

wrt_insmod() {
    kbit=$1
    kv=$(uname -r)
    
    echo "insmod $kv $kbit" >> $VTOY_PATH/log
    
    [ -f /ventoy_openwrt/$kv/$kbit/dax.ko ] && insmod /ventoy_openwrt/$kv/$kbit/dax.ko > /dev/null 2>&1
    [ -f /ventoy_openwrt/$kv/$kbit/dm-mod.ko ] && insmod /ventoy_openwrt/$kv/$kbit/dm-mod.ko > /dev/null 2>&1
}


mkdir /sys
mount -t sysfs sys /sys
mdev -s


if [ -f /ventoy_openwrt.xz ]; then
    tar xf /ventoy_openwrt.xz -C /
    rm -f  /ventoy_openwrt.xz
fi


if uname -m | egrep -q "amd64|x86_64"; then
    wrt_insmod 64
else
    wrt_insmod generic    
    if lsmod | grep -q 'dm-mod'; then
        echo "insmod generic failed" >> $VTOY_PATH/log
    else
        wrt_insmod legacy
    fi
fi

sh $VTOY_PATH/loop/openwrt/ventoy-disk.sh
