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

CD_DETECT="/var/lib/dpkg/info/cdrom-detect.postinst"

if [ -e /init ] && $GREP -q '^mountroot$' /init; then
    echo "Here before mountroot ..." >> $VTLOG
    
    $SED  "/^mountroot$/i\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/disk_mount_hook.sh"  -i /init
    $SED  "/^mountroot$/i\\export LIVEMEDIA=/dev/mapper/ventoy"  -i /init
    $SED  "/^mountroot$/i\\export LIVE_MEDIA=/dev/mapper/ventoy"  -i /init    

    if $GREP -q 'live-media=' /proc/cmdline; then
        if [ -f /scripts/casper ] && $GREP -q '^  *LIVEMEDIA=' /scripts/casper; then
            $SED "s#^  *LIVEMEDIA=.*#LIVEMEDIA=/dev/mapper/ventoy#" -i /scripts/casper
        fi
    fi
elif [ -e "$CD_DETECT" ]; then
    echo "$CD_DETECT exist, now add hook in it..." >> $VTLOG
    $SED  "1 a $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/disk_mount_hook.sh"  -i "$CD_DETECT"
    
    if [ -e /bin/list-devices ]; then
        mv /bin/list-devices /bin/list-devices-bk
        cp -a /ventoy/hook/debian/list-devices /bin/list-devices
    fi
elif [ -e /init ] && $GREP -q '/start-udev$' /init; then
    echo "Here use notify ..." >> $VTLOG
    
    ventoy_set_inotify_script  debian/ventoy-inotifyd-hook.sh
    $SED  "/start-udev$/i\\mount -n -o mode=0755 -t devtmpfs devtmpfs /dev"  -i /init
    $SED  "/start-udev$/i\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/default/ventoy-inotifyd-start.sh"  -i /init
else
    echo "Here use udev hook ..." >> $VTLOG
    ventoy_systemd_udevd_work_around
    ventoy_add_udev_rule "$VTOY_PATH/hook/debian/udev_disk_hook.sh %k"
fi

if [ -f $VTOY_PATH/autoinstall ]; then
    echo "Do auto install ..." >> $VTLOG
    
    if $GREP -q '^autoinstall:' $VTOY_PATH/autoinstall; then
        echo "cloud-init auto install ..." >> $VTLOG
        if $GREP -q "maybe_break init" /init; then
            $SED "/maybe_break init/i $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/ventoy-cloud-init.sh \$rootmnt"  -i /init
        fi
    else
        if $GREP -q "^mount /proc$" /init; then
            $SED "/^mount \/proc/a export file=$VTOY_PATH/autoinstall; export auto='true'; export priority='critical'"  -i /init
        fi
    fi
    
    # if [ -e /bin/check-missing-firmware ]; then
    #     $SED "/^#!/a\exit 0" -i /bin/check-missing-firmware
    # fi
fi

#for ARMA aka Omoikane
if [ -f /mod.img ] && [ -f /mod/fs/cramfs.ko ]; then
    echo "mount mod.img and install dm-mod.ko" >> $VTLOG
    $BUSYBOX_PATH/insmod /mod/fs/cramfs.ko
    
    $BUSYBOX_PATH/mkdir $VTOY_PATH/modmnt
    $BUSYBOX_PATH/mount /mod.img $VTOY_PATH/modmnt
    $BUSYBOX_PATH/insmod $VTOY_PATH/modmnt/md/dm-mod.ko
    $BUSYBOX_PATH/umount $VTOY_PATH/modmnt
    
    $BUSYBOX_PATH/rmmod cramfs
fi

#for siduction-patience-nox-
if [ -f /scripts/fll ]; then
    $SED  "/unset FINGERED/a\\echo '/dev/mapper/ventoy';return;"  -i /scripts/fll
fi

