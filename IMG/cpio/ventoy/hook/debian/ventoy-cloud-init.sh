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

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

newroot=$1

dd if=/dev/zero of=$newroot/tmp/cidata.img bs=1M count=8 status=none
freeloop=$(losetup -f)

losetup $freeloop $newroot/tmp/cidata.img

mkfs.vfat -n CIDATA $freeloop

mkdir /tmpcidata
mount $newroot/tmp/cidata.img /tmpcidata

vtSplit=$(grep VENTOY_META_DATA_SPLIT $VTOY_PATH/autoinstall | wc -l)
if [ $vtSplit -eq 1 ]; then
    vtlog "split autoinstall script to user-data and meta-data"
    vtLine=$(grep -n VENTOY_META_DATA_SPLIT $VTOY_PATH/autoinstall | awk -F: '{print $1}')
    vtLine1=$(expr $vtLine - 1)
    vtLine2=$(expr $vtLine + 1)
    vtlog "Line number: $vtLine $vtLine1 $vtLine2"
    sed -n "1,${vtLine1}p"  $VTOY_PATH/autoinstall >/tmpcidata/user-data
    sed -n "${vtLine2},\$p" $VTOY_PATH/autoinstall >/tmpcidata/meta-data
else
    vtlog "only user-data avaliable"
    cp -a $VTOY_PATH/autoinstall  /tmpcidata/user-data
    touch /tmpcidata/meta-data
fi


umount /tmpcidata
rm -rf /tmpcidata

vtCMD=$(cat /proc/cmdline)
echo "autoinstall $vtCMD" > $newroot/tmp/kcmdline
mount --bind $newroot/tmp/kcmdline  /proc/cmdline


PATH=$VTPATH_OLD

