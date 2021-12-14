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

$SED "/maybe_break premount/i\ $BUSYBOX_PATH/sh $VTOY_PATH/loop/esysrescue/ventoy-disk.sh" -i /init

if [ -f /scripts/casper-bottom/09format_esr_data_partition ]; then
    $SED '/mkfs.vfat.*edev.3/icp -a /dev/dm-3 /dev/ventoy3' -i /scripts/casper-bottom/09format_esr_data_partition
fi
