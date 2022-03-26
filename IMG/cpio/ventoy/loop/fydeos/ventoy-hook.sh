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

ventoy_create_chromeos_ventoy_part 3
mdev -s

vtlog "copy out the e2fsck program ..."

copy_lib() {
    cp -a /ventoy_rdroot/usr/lib64/$1 /usr/lib64/
    cp -a /ventoy_rdroot/lib64/$1 /lib64/
}

mkdir /ventoy_rdroot
mkdir -p /lib /lib64 /usr/lib64 /sbin

mount -o ro /dev/ventoy3 /ventoy_rdroot >>$VTLOG 2>&1
cp -a /ventoy_rdroot/sbin/e2fsck /sbin/
cp -a /ventoy_rdroot/sbin/dmsetup /sbin/

copy_lib libext2fs*
copy_lib libcom_err*
copy_lib libe2p*
copy_lib libblk*
copy_lib libuuid*
copy_lib libdl.*
copy_lib libdl-*
copy_lib libc.*
copy_lib libc-*
copy_lib libpthread*
copy_lib ld-*
copy_lib libdevmapper*
copy_lib libudev*
copy_lib libm.*
copy_lib libm-*
copy_lib librt*
copy_lib libpopt*
copy_lib libgpg-error*
copy_lib libselinux*
copy_lib libsepol*
copy_lib libpcre*
copy_lib libcap*
copy_lib libdw*
copy_lib libgcc_s*
copy_lib libattr*
copy_lib libelf*
copy_lib libz.*
copy_lib libbz2*
copy_lib libgcrypt*
copy_lib liblvm*

ln -s /lib64/libdevmapper.so.1.02 /lib64/libdevmapper.so.1.02.1

umount /ventoy_rdroot

vtlog "========================================="
vtlog "===== e2fsck -y -v /dev/ventoy1 ====="
e2fsck -y -v /dev/ventoy1 >>$VTLOG 2>&1
#vtlog "===== e2fsck -y -v /dev/ventoy3 ====="
#e2fsck -y -v /dev/ventoy3 >>$VTLOG 2>&1
vtlog "===== e2fsck -y -v /dev/ventoy8 ====="
e2fsck -y -v /dev/ventoy8 >>$VTLOG 2>&1
vtlog "========================================="

/sbin/dmsetup --version >>$VTLOG 2>&1
veritysetup --version >>$VTLOG 2>&1

vtlog "proc devtmpfs ..."
mkdir /newdev
mount -t devtmpfs dev /newdev

cp -a /dev/mapper/ventoy* /newdev/mapper/
cp -a /dev/ventoy* /newdev/

if [ "$VTOY_VLNK_BOOT" = "01" ]; then
    vtLine=$($VTOY_PATH/tool/vtoydump -f /ventoy/ventoy_os_param)
    vtdiskname=${vtLine%%#*}
fi

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


# if grep -q 'DM=' /proc/cmdline; then
    # vtlog "Boot verified image ..."
    
    # dmP1=$(sed "s/.*\(0 [0-9]* verity\).*/\1/" /proc/cmdline)
    # alg=$(sed "s/.*alg=\([^ ]*\).*/\1/" /proc/cmdline)
    # hexdigest=$(sed "s/.*root_hexdigest=\([0-9a-fA-F][0-9a-fA-F]*\).*/\1/" /proc/cmdline)
    # salt=$(sed "s/.*salt=\([0-9a-fA-F][0-9a-fA-F]*\).*/\1/" /proc/cmdline)
    # hashstart=$(sed "s/.*hashstart=\([0-9][0-9]*\).*/\1/" /proc/cmdline)
    
    #512 to 4096
    # blocknum=$(expr $hashstart / 8)
    # hashoffset=$(expr $hashstart \* 512)
    
    # vtlog "veritysetup create vroot $vt_root_dev $vt_root_dev $hexdigest --data-block-size=4096 --hash-block-size=4096 --data-blocks=$blocknum --hash-offset=$hashoffset --salt=$salt --hash=$alg --no-superblock --format=0"
    # veritysetup create vroot $vt_root_dev $vt_root_dev $hexdigest --data-block-size=4096 --hash-block-size=4096 --data-blocks=$blocknum --hash-offset=$hashoffset --salt=$salt --hash=$alg --no-superblock --format=0
    # sleep 1
    # mdev -s

    # blkdev_num=$(dmsetup ls | grep vroot | sed 's/.*(\([0-9][0-9]*\),[^0-9]*\([0-9][0-9]*\).*/\1:\2/')
    # vtDM=$(ventoy_find_dm_id ${blkdev_num})
    # vtlog "blkdev_num=$blkdev_num vtDM=$vtDM"

    # if [ -b /dev/$vtDM ]; then
        # veritysetup status vroot >> $VTLOG 2>&1
        # mount -o ro /dev/$vtDM /ventoy_rdroot
    # else
        # mount -o ro $vt_root_dev /ventoy_rdroot
    # fi
# else
    # vtlog "Boot normal image ..."
    # mount -o ro $vt_root_dev /ventoy_rdroot
# fi

vtlog "Boot normal image ..."
mount -o ro $vt_root_dev /ventoy_rdroot

cp -a $VTLOG /newdev/ventoy.log
umount /newdev
mount -t devtmpfs dev /ventoy_rdroot/dev

PATH=$VTPATH_OLD
