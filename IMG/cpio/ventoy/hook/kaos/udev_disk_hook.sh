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

if is_ventoy_hook_finished || not_ventoy_disk "${1:0:-1}"; then
    exit 0
fi

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

modprobe fuse
mkdir -p $VTOY_PATH/mnt/fuse $VTOY_PATH/mnt/iso

vtoydm -p -f $VTOY_PATH/ventoy_image_map -d "/dev/${1:0:-1}" > $VTOY_PATH/ventoy_dm_table
vtoy_fuse_iso -f $VTOY_PATH/ventoy_dm_table -m $VTOY_PATH/mnt/fuse
mount -t iso9660 $VTOY_PATH/mnt/fuse/ventoy.iso  $VTOY_PATH/mnt/iso

# OK finish
set_ventoy_hook_finish

PATH=$VTPATH_OLD

