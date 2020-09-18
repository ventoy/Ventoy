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

PATH=$PATH:$VTOY_PATH/busybox

mkdir /ventoy_rdroot

mknod -m 660 /ram0 b 1 0

dd if=/initrd001 of=/ram0
rm -f /initrd001

mount /ram0 /ventoy_rdroot

vtSize=$(du -m -s $VTOY_PATH | awk '{print $1}')
let vtSize=vtSize+4

mkdir -p /ventoy_rdroot/ventoy
mount -t tmpfs -o size=${vtSize}m tmpfs /ventoy_rdroot/ventoy
cp -a /ventoy/* /ventoy_rdroot/ventoy/


cd /ventoy_rdroot

sed "/scan_cdroms *$/i $BUSYBOX_PATH/sh $VTOY_PATH/hook/smgl/disk_hook.sh" -i sbin/smgl.init


