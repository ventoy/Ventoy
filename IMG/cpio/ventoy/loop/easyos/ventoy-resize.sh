#!/ventoy/busybox/sh
#************************************************************************************
# Copyright (c) 2022, longpanda <admin@ventoy.net>
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

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')
vtDM0=$(ventoy_find_dm_id ${blkdev_num})
blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy2 | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')
vtDM2=$(ventoy_find_dm_id ${blkdev_num})
vtlog "vtDM0=$vtDM0 vtDM2=$vtDM2"

vtSize=$(cat /sys/block/$vtDM0/size)
vtSize1=$(sed -n "1p" /vtoy_dm_table | awk '{print $2}')
vtStart1=$(sed -n "1p" /vtoy_dm_table | awk '{print $5}')
vtSize2=$(sed -n "2p" /vtoy_dm_table | awk '{print $2}')
vtNewSize2=$(expr $vtSize - $vtSize1 - $vtStart1)
vtlog "vtSize=$vtSize vtSize1=$vtSize1 vtStart1=$vtStart1 vtSize2=$vtSize2 vtNewSize2=$vtNewSize2"


sed -n "2p" /vtoy_dm_table > /ventoy/resize_table
sed -i "s/$vtSize2/$vtNewSize2/" /ventoy/resize_table

dmsetup remove ventoy2
dmsetup create ventoy2 /ventoy/resize_table

