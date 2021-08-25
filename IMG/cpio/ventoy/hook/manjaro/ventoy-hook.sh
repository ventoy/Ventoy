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

# some distro initramfs doesn't contain device-mapper udev rules file
DISTRO_UDEV_DIR=$(ventoy_get_udev_conf_dir)
if [ -s "$DISTRO_UDEV_DIR/13-dm-disk.rules" ]; then
    echo 'dm-disk rule exist' >> $VTLOG
else
    echo 'Copy dm-disk rule file' >> $VTLOG
    $CAT $VTOY_PATH/hook/default/13-dm-disk.rules > "$DISTRO_UDEV_DIR/13-dm-disk.rules"
fi

if $GREP -q '^"$mount_handler"' /init; then
    echo 'use mount_handler ...' >> $VTLOG
    
    vthookfile=/hooks/archiso
    
    if [ -e /hooks/miso ]; then
        vthookfile=/hooks/miso
        $SED "/^\"\$mount_handler\"/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/manjaro/ventoy-disk.sh \"\$misodevice\"" -i /init    
    else
        $SED "/^\"\$mount_handler\"/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/manjaro/ventoy-disk.sh \"\$archisodevice\"" -i /init
    fi
    
    if [ -f $vthookfile ]; then
        $SED  '/while ! poll_device "${dev}"/a\    if /ventoy/busybox/sh /ventoy/hook/manjaro/ventoy-timeout.sh ${dev}; then break; fi'   -i $vthookfile
    fi   
elif $GREP -q '^mount_setup$' init; then
    echo "Here use notify ..." >> $VTLOG
    
    ventoy_set_inotify_script  manjaro/ventoy-inotifyd-hook.sh
    $SED  "/^mount_setup$/a\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/default/ventoy-inotifyd-start.sh"  -i /init
else
    ventoy_systemd_udevd_work_around
    ventoy_add_udev_rule "$VTOY_PATH/hook/default/udev_disk_hook.sh %k"
fi
