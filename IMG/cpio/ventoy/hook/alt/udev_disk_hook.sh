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

ventoy_create_alt_img_mapper() {
    read -r zero size target parent start <<EOF_RANGE
    $($SED -n "${2}p" /vtoy_dm_table)
EOF_RANGE

    map="$VTOY_PATH/ventoy_dm_table"
    root_table="$VTOY_PATH/alt_img_dm_table"
    ventoy_check_dm_module || return 1
    "$VTOY_PATH/tool/vtoydm" -p -f "$VTOY_PATH/ventoy_image_map" -d "$1" > "$map" || return 1
    $AWK -v start="$start" -v size="$size" '
        {
            left = ($1 > start ? $1 : start)
            right = ($1 + $2 < start + size ? $1 + $2 : start + size)
            if (left < right) {
                if ($3 != "linear" || left != start + covered) {
                    exit 1
                }
                printf "%.0f %.0f linear %s %.0f\n", covered, right - left, $4, $5 + left - $1
                covered += right - left
            }
        }
        END { if (!size || covered != size) exit 1 }
    ' "$map" > "$root_table" || return 1

    "$VTOY_PATH/tool/dmsetup" --readonly create ventoy "$root_table" >>"$VTLOG" 2>&1
}

disk=$(ventoy_partname_to_diskname "$1")
if is_ventoy_hook_finished || not_ventoy_disk "$disk"; then
    exit 0
fi

part=$($SED -n 's/.* ventoy_alt_img_part=\([1234]\)\( .*\)\?$/\1/p' /proc/cmdline)
if [ -n "$part" ] && [ -s /vtoy_dm_table ]; then
    if ventoy_create_alt_img_mapper "/dev/$disk" "$part"; then
        set_ventoy_hook_finish
    fi
    exit
fi

ventoy_udev_disk_common_hook $* "noreplace"

if ! [ -e $VTOY_DM_PATH ]; then
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    mknod -m 0666 $VTOY_DM_PATH b $blkdev_num
fi

# 
# We do a trick for ATL series here.
# Use /dev/vtCheatLoop and wapper it as a cdrom with bind mount.
# Then the installer will accept /dev/vtCheatLoop as the install medium.
#
vtCheatLoop=loop6
ventoy_copy_device_mapper  /dev/$vtCheatLoop
$BUSYBOX_PATH/mkdir -p /tmp/$vtCheatLoop/device/
echo 5 > /tmp/$vtCheatLoop/device/type
$BUSYBOX_PATH/mount --bind /tmp/$vtCheatLoop /sys/block/$vtCheatLoop >> $VTLOG 2>&1


# OK finish
set_ventoy_hook_finish

