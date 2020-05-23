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

if $GREP -q 'FILTERED_LIST=[^a-zA-Z0-9_]*$' /init; then
    $SED 's#FILTERED_LIST=[^a-zA-Z0-9_]*$#FILTERED_LIST=/dev/mapper/ventoy#' -i /init
elif $GREP -q '\[ "$FILTERED_LIST" \]' /init; then
    $SED '/\[ "$FILTERED_LIST" \]/i\    FILTERED_LIST="/dev/mapper/ventoy $FILTERED_LIST"' -i /init
fi

$SED -i "/_search_for_boot_device_/a\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/antix-disk.sh" /init

if [ -f $VTOY_PATH/ventoy_persistent_map ]; then
    $SED 's#for param in $cmdline#for param in persist_all $cmdline#g' -i /init
fi

# for debug
#$SED -i "/^linuxfs_error/a\exec $VTOY_PATH/busybox/sh" /init
