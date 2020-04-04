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

# Step 1: dd initrd file to ramdisk
vtRamdiskFile=$($BUSYBOX_PATH/ls /initrd* | $HEAD -n1)
$BUSYBOX_PATH/mknod -m 0666 /ram0 b 1 0
$BUSYBOX_PATH/dd if=$vtRamdiskFile of=/ram0 status=none
$BUSYBOX_PATH/rm -f $vtRamdiskFile

# Step 2: mount ramdisk
$BUSYBOX_PATH/mkdir -p /ventoy_rdroot
ventoy_close_printk
$BUSYBOX_PATH/mount /ram0 /ventoy_rdroot
ventoy_restore_printk

# Step 3: Copy ventoy tool to new root directory.
# Here we make a tmpfs mount to avoid ramdisk out of space (additional space is for log).
vtSize=$($BUSYBOX_PATH/du -m -s $VTOY_PATH | $BUSYBOX_PATH/awk '{print $1}')
let vtSize=vtSize+4

$BUSYBOX_PATH/mkdir -p /ventoy_rdroot/ventoy
$BUSYBOX_PATH/mount -t tmpfs -o size=${vtSize}m tmpfs /ventoy_rdroot/ventoy
$BUSYBOX_PATH/cp -a /ventoy/* /ventoy_rdroot/ventoy/


# Step 4: add hook in linuxrc&rc.sysinit script file
vtLine=$($GREP -n "^find_cdrom" /ventoy_rdroot/linuxrc | $GREP -v '(' | $AWK -F: '{print $1}')
$SED "$vtLine aif test -d /ventoy; then $BUSYBOX_PATH/sh $VTOY_PATH/hook/pclos/disk_hook.sh; fi" -i /ventoy_rdroot/linuxrc
$SED "$vtLine aif test -d /initrd/ventoy; then ln -s /initrd/ventoy /ventoy; fi" -i /ventoy_rdroot/linuxrc


vtRcInit=$($BUSYBOX_PATH/tail /ventoy_rdroot/linuxrc | $GREP 'exec ' | $AWK '{print $2}')
if [ -e /ventoy_rdroot$vtRcInit ]; then
    vtRcInit=/ventoy_rdroot$vtRcInit
else
    vtRcInit=/ventoy_rdroot/etc/rc.d/rc.sysinit
fi

echo 'exec /sbin/init' >> $vtRcInit
