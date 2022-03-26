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

if $GREP -q '^"$mount_handler"' /init; then
    echo 'use mount_handler1 ...' >> $VTLOG
    
    vthookfile=/hooks/archiso
    
    if [ -e /hooks/miso ]; then
        vthookfile=/hooks/miso
        $SED "/^\"\$mount_handler\"/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/arch/ventoy-disk.sh \"\$misodevice\"" -i /init
    elif [ -e /hooks/artix ]; then
        vthookfile=/hooks/artix
        $SED "/^\"\$mount_handler\"/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/arch/ventoy-disk.sh \"\$artixdevice\"" -i /init
    else
        $SED "/^\"\$mount_handler\"/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/arch/ventoy-disk.sh \"\$archisodevice\"" -i /init
    fi
    
    if [ -f $vthookfile ]; then
        $SED  '/while ! poll_device "${dev}"/a\    if /ventoy/busybox/sh /ventoy/hook/arch/ventoy-timeout.sh ${dev}; then break; fi'   -i $vthookfile
    fi
elif $GREP -q '^$mount_handler' /init; then
    echo 'use mount_handler2 ...' >> $VTLOG
    $SED "/^\$mount_handler/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/arch/ventoy-disk.sh" -i /init
    
elif $GREP -q '^KEEP_SEARCHING' /init; then
    echo 'KEEP_SEARCHING found ...' >> $VTLOG
    $SED "/^KEEP_SEARCHING/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/arch/ovios-disk.sh " -i /init
    
    $BUSYBOX_PATH/mkdir -p /dev    
    $BUSYBOX_PATH/mkdir -p /sys
    $BUSYBOX_PATH/mount -t sysfs sys /sys
    
else
    # some archlinux initramfs doesn't contain device-mapper udev rules file
    ARCH_UDEV_DIR=$(ventoy_get_udev_conf_dir)
    if [ -s "$ARCH_UDEV_DIR/13-dm-disk.rules" ]; then
        echo 'dm-disk rule exist' >> $VTLOG
    else
        echo 'Copy dm-disk rule file' >> $VTLOG
        $CAT $VTOY_PATH/hook/default/13-dm-disk.rules > "$ARCH_UDEV_DIR/13-dm-disk.rules"
    fi

    # use default proc
    ventoy_systemd_udevd_work_around

    ventoy_add_udev_rule "$VTOY_PATH/hook/default/udev_disk_hook.sh %k"
fi

if [ -f $VTOY_PATH/ventoy_persistent_map ]; then
    $SED "1 aexport cow_label=vtoycow" -i /init 
fi
