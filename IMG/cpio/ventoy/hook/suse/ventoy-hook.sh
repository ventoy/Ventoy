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

if [ -f $VTOY_PATH/autoinstall ]; then    
    if [ -f /linuxrc.config ]; then
        echo "AutoYaST: file:///ventoy/autoinstall" >> /info-ventoy
        $SED "1 iinfo: file:/info-ventoy" -i /linuxrc.config
    fi
fi

if $GREP -q 'rdinit=/vtoy/vtoy' /proc/cmdline; then    
    echo "remove rdinit param" >> $VTLOG
    echo "ptoptions=+rdinit" >> /linuxrc.config
fi


if $BUSYBOX_PATH/ls $VTOY_PATH | $GREP -q 'ventoy_dud[0-9]'; then
    if [ -f /linuxrc.config ]; then
        vtKerVer=$($BUSYBOX_PATH/uname -r)
        ventoy_check_insmod /modules/loop.ko
        ventoy_check_insmod /modules/squashfs.ko
        
        ventoy_check_mount /parts/00_lib /modules
        ventoy_check_insmod /modules/lib/modules/$vtKerVer/initrd/isofs.ko
        $BUSYBOX_PATH/umount /modules
    
        for vtDud in $($BUSYBOX_PATH/ls $VTOY_PATH/ventoy_dud*); do
            $BUSYBOX_PATH/mkdir -p ${vtDud%.*}_mnt
            if $BUSYBOX_PATH/mount $vtDud ${vtDud%.*}_mnt > /dev/null 2>&1; then
                $BUSYBOX_PATH/cp -a ${vtDud%.*}_mnt  ${vtDud%.*}_data
                $BUSYBOX_PATH/umount ${vtDud%.*}_mnt
                echo "dud: file://${vtDud%.*}_data" >> /linuxrc.config
            else
                echo "mount $vtDud failed" >> $VTLOG
            fi
        done
        
        $BUSYBOX_PATH/rmmod isofs >> $VTLOG 2>&1
        $BUSYBOX_PATH/rmmod squashfs >> $VTLOG 2>&1
        $BUSYBOX_PATH/rmmod loop >> $VTLOG 2>&1
    fi
fi

#echo "Exec: /bin/sh $VTOY_PATH/hook/suse/cdrom-hook.sh" >> /info-ventoy
#echo "install: hd:/?device=/dev/mapper/ventoy" >> /info-ventoy
#$SED "1 iinfo: file:/info-ventoy" -i /linuxrc.config

if [ -e /etc/initrd.functions ] && $GREP -q 'HPIP' /etc/initrd.functions; then
    echo "HPIP" >> $VTLOG    
    $BUSYBOX_PATH/mkdir /dev
    $BUSYBOX_PATH/mknod -m 660 /dev/console c 5 1
    $SED "/CD_DEVICES=/a $BUSYBOX_PATH/sh $VTOY_PATH/hook/suse/disk_hook.sh" -i /etc/initrd.functions
    $SED "/CD_DEVICES=/a CD_DEVICES=\"/dev/ventoy \$CD_DEVICES\"" -i /etc/initrd.functions
elif [ -f /scripts/udev_setup ]; then
    echo "udev_setup" >> $VTLOG
    echo "/ventoy/busybox/sh /ventoy/hook/suse/udev_setup_hook.sh" >> /scripts/udev_setup    
else
    echo "SUSE" >> $VTLOG
    ventoy_systemd_udevd_work_around
    ventoy_add_udev_rule "$VTOY_PATH/hook/suse/udev_disk_hook.sh %k"
fi
