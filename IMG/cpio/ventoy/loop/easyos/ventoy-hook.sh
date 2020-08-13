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

$SED "/find drives/i $BUSYBOX_PATH/sh $VTOY_PATH/loop/easyos/ventoy-disk.sh; vtDM=\$(cat /ventoy/vtDM)" -i /init

$SED "1a boot_dev=ventoy1;wkg_dev=ventoy2" -i /init

#check for ssd will read /sys/block/ventoy, will no exist, need a workaround
$SED "s#/sys/block/\${WKG_DRV}/#/sys/block/\$vtDM/#g"  -i /init

#skip the resizing process, can't resizing partition
$SED "s#640M#0M#g"  -i /init


