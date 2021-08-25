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

echo "CDlinux process..." >> $VTLOG

$BUSYBOX_PATH/mknod -m 0660 /ventoy/ram0 b 1 0

$BUSYBOX_PATH/mkdir /vtmnt /ventoy_rdroot
$BUSYBOX_PATH/mount -t squashfs /ventoy/ram0 /vtmnt

$BUSYBOX_PATH/mount -nt tmpfs -o mode=755 tmpfs /ventoy_rdroot

$BUSYBOX_PATH/cp -a /vtmnt/* /ventoy_rdroot
$BUSYBOX_PATH/ls -1a /vtmnt/ | $GREP '^\.[^.]' | while read vtLine; do
    $BUSYBOX_PATH/cp -a /vtmnt/$vtLine /ventoy_rdroot
done

$BUSYBOX_PATH/umount /vtmnt && $BUSYBOX_PATH/rm -rf /vtmnt
$BUSYBOX_PATH/cp -a /ventoy /ventoy_rdroot

if [ -f /etc/default/cdlinux ]; then
    echo "CDL_WAIT=60" >> /etc/default/cdlinux
fi

echo 'echo "CDL_DEV=/dev/mapper/ventoy" >>"$VAR_FILE"' >> /ventoy_rdroot/etc/rc.d/rc.var

ventoy_set_rule_dir_prefix /ventoy_rdroot
ventoy_systemd_udevd_work_around
ventoy_add_udev_rule "$VTOY_PATH/hook/default/udev_disk_hook.sh %k noreplace"
