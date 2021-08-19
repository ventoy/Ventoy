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

check_mkdev_node() {
    for i in $(ls /sys/class/block/); do
        if ! [ -e /dev/$i ]; then
            blkdev_num=$(sed 's/:/ /g' /sys/class/block/$i/dev)
            vtlog "mknod -m 0666 /dev/$i b $blkdev_num"
            mknod -m 0666 /dev/$i b $blkdev_num
        fi
    done
}

check_insmod() {
    if [ -f "$1" ]; then
        vtlog "insmod $1"
        insmod "$1" >> $VTOY_PATH/log 2>&1
    else
        vtlog "$1 not exist"
    fi
}

wrt_insmod() {
    kbit=$1
    kv=$(uname -r)
    
    vtlog "insmod $kv $kbit"
    
    check_insmod /ventoy_openwrt/$kv/$kbit/dax.ko
    check_insmod /ventoy_openwrt/$kv/$kbit/dm-mod.ko    
}

insmod_dm_mod() {
    if grep -q "device-mapper" /proc/devices; then
        vtlog "device-mapper enabled by system 0"
        return
    fi
    
    check_insmod /ventoy/modules/dax.ko
    check_insmod /ventoy/modules/dm-mod.ko

    if grep -q "device-mapper" /proc/devices; then
        vtlog "device-mapper enabled by system 1"
        return
    fi
    
    if [ -f /ventoy_openwrt.xz ]; then
        tar xf /ventoy_openwrt.xz -C /
        rm -f  /ventoy_openwrt.xz
    fi

    if uname -m | egrep -q "amd64|x86_64"; then
        wrt_insmod 64
    else
        wrt_insmod generic    
        if lsmod | grep -q 'dm-mod'; then
            vterr "insmod generic failed"
        else
            wrt_insmod legacy
        fi
    fi
}

insmod_dm_mod

check_mkdev_node
sleep 1

while [ -n "Y" ]; do
    vtusb_disk=$(get_ventoy_disk_name)
    if check_usb_disk_ready "$vtusb_disk"; then
        vtlog "get_ventoy_disk_name $vtusb_disk ready"
        break;
    else
        vtlog "get_ventoy_disk_name $vtusb_disk not ready"
        sleep 2
        check_mkdev_node
    fi    
done


vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')
vtDM=$(ventoy_find_dm_id ${blkdev_num})
echo -n $vtDM > /ventoy/vtDM

ventoy_create_dev_ventoy_part
mdev -s
check_mkdev_node


mkdir /ventoy_rdroot
mount /dev/ventoy2 /ventoy_rdroot

PATH=$VTPATH_OLD

set_ventoy_hook_finish
