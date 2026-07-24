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

TRANSFILE=/usr/Euler/project/load/filetransfer.sh

LINE=$($GREP -n 'function ft_mountCdromDir' $TRANSFILE | $AWK -F':' '{print $1}')

echo "LINE=$LINE" >> $VTLOG

$SED 's#FT_SR_DEV=.*#FT_SR_DEV=/dev/mapper/ventoy#g' -i $TRANSFILE
$SED 's/function ft_mountCdromDir/function ft_mountCdromDirXX/' -i $TRANSFILE

let LINE--
$SED -n "1,${LINE}p" $TRANSFILE >> /ventoy/filetransfer.sh

$CAT $VTOY_PATH/hook/euleros/hook_func.sh >> /ventoy/filetransfer.sh

let LINE++
$SED -n "${LINE},\$p" $TRANSFILE >> /ventoy/filetransfer.sh

$BUSYBOX_PATH/mv /ventoy/filetransfer.sh $TRANSFILE

#fix the euler os shell script
$SED 's/-z ${harddisk_disk_list}/-z "${harddisk_disk_list}"/' -i /usr/Euler/project/disk/disk_tool.sh
