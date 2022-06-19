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

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    exit 0
fi

vtlog "wait_for_usb_disk_ready $vtdiskname ..."

if echo $vtdiskname | $EGREP -q "nvme|mmc|nbd"; then
    vtpart2=${vtdiskname}p2
else
    vtpart2=${vtdiskname}2
fi

/ventoy/busybox/sh /ventoy/hook/suse/udev_disk_hook.sh "${vtpart2#/dev/}"

if $GREP -q 'mediacheck=1' /proc/cmdline; then   
    ventoy_copy_device_mapper "${vtdiskname}"
fi

if [ -f /ventoy/autoinstall ]; then
    sh /ventoy/hook/default/auto_install_varexp.sh  /ventoy/autoinstall
fi
if [ -f /autoinst.xml ]; then
    sh /ventoy/hook/default/auto_install_varexp.sh  /autoinst.xml
fi

