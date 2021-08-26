#!/ventoy/busybox/sh
#************************************************************************************
# Copyright (c) 2021, longpanda <admin@ventoy.net>
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

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

mkdir /sys
mount -t sysfs sys /sys
mdev -s
sleep 2

while [ -n "Y" ]; do
    usb_disk=$(get_ventoy_disk_name)
    
    if echo $usb_disk | egrep -q "nvme|mmc|nbd"; then
        vtpart2=${usb_disk}p2
    else
        vtpart2=${usb_disk}2
    fi
    
    if [ -e "${vtpart2}" ]; then
        break
    else
        sleep 2
        mdev -s
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

vtlog "copy out the e2fsck program ..."
mkdir /ventoy_rdroot
mkdir -p /lib /lib64 /usr/lib64 /sbin

mount -o ro /dev/ventoy3 /ventoy_rdroot >>$VTLOG 2>&1
cp -a /ventoy_rdroot/sbin/e2fsck /sbin/
cp -a /ventoy_rdroot/usr/lib64/libext2fs* /usr/lib64/
cp -a /ventoy_rdroot/usr/lib64/libcom_err* /usr/lib64/
cp -a /ventoy_rdroot/usr/lib64/libe2p* /usr/lib64/
cp -a /ventoy_rdroot/lib64/libblk* /lib64/
cp -a /ventoy_rdroot/lib64/libuuid* /lib64/
cp -a /ventoy_rdroot/lib64/libdl.* /lib64/
cp -a /ventoy_rdroot/lib64/libdl-* /lib64/
cp -a /ventoy_rdroot/lib64/libc.* /lib64/
cp -a /ventoy_rdroot/lib64/libc-* /lib64/
cp -a /ventoy_rdroot/lib64/libpthread* /lib64/
cp -a /ventoy_rdroot/lib64/ld-* /lib64/

umount /ventoy_rdroot

vtlog "========================================="
vtlog "===== e2fsck -y -v /dev/ventoy1 ====="
e2fsck -y -v /dev/ventoy1 >>$VTLOG 2>&1
vtlog "===== e2fsck -y -v /dev/ventoy3 ====="
e2fsck -y -v /dev/ventoy3 >>$VTLOG 2>&1
vtlog "===== e2fsck -y -v /dev/ventoy8 ====="
e2fsck -y -v /dev/ventoy8 >>$VTLOG 2>&1
vtlog "========================================="

vtlog "proc devtmpfs ..."
mkdir /newdev
mount -t devtmpfs dev /newdev

cp -a /dev/mapper/ventoy* /newdev/mapper/
cp -a /dev/ventoy* /newdev/


vtshortname="${vtdiskname#/dev/}"
mv /newdev/${vtshortname} /newdev/backup_${vtshortname}
cp -a /dev/ventoy /newdev/${vtshortname}

for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if [ -e /dev/ventoy${i} ]; then
        if echo $vtdiskname | egrep -q "nvme|mmc|nbd"; then
            vtpart=p$i
        else
            vtpart=$i
        fi
        
        if [ -e /newdev/${vtshortname}${vtpart} ]; then
            mv /newdev/${vtshortname}${vtpart} /newdev/backup_${vtshortname}${vtpart}
        fi

        cp -a /dev/ventoy${i} /newdev/${vtshortname}${vtpart}
        
        if [ $i -eq 3 ]; then
            [ -e /dev/${vtshortname}${vtpart} ] && rm -f /dev/${vtshortname}${vtpart}
            cp -a /dev/ventoy${i} /dev/${vtshortname}${vtpart}            
            vt_root_dev="/dev/${vtshortname}${vtpart}"            
            vtlog "vt_root_dev=$vt_root_dev"
        fi
    fi
done

cp -a $VTLOG /newdev/ventoy.log
umount /newdev

mount -o ro $vt_root_dev /ventoy_rdroot
mount -t devtmpfs dev /ventoy_rdroot/dev

PATH=$VTPATH_OLD
