#!/ventoy/busybox/sh
#************************************************************************************
# Copyright (c) 2021, longpanda <admin@ventoy.net>
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

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

get_rhel_ver() {
    if uname -m | grep -q '64'; then
        machine='_X64'
    fi

    if grep -q '6[.]1' /etc/redhat-release; then
        echo "RHAS6U1$machine"; return
    fi
    
    echo "RHAS6U1$machine"
}

install_dm_mod_ko() {
    # dump iso file location
    vtoydm -i -f $VTOY_PATH/ventoy_image_map -d ${vtdiskname} > $VTOY_PATH/iso_file_list

    sysver=$(get_rhel_ver)
    vtlog "sysver=$sysver"

    LINE=$(grep "$sysver" -n -m1 $VTOY_PATH/iso_file_list | awk -F: '{print $1}')
    vtlog "LINE=$LINE"

    LINE=$(sed -n "$LINE,\$p" $VTOY_PATH/iso_file_list | grep -m1 'initrd.img')
    vtlog "LINE=$LINE"

    sector=$(echo $LINE | $AWK '{print $(NF-1)}')
    length=$(echo $LINE | $AWK '{print $NF}')
    vtlog "sector=$sector  length=$length"

    mkdir xxx
    vtoydm -e -f $VTOY_PATH/ventoy_image_map -d ${vtdiskname} -s $sector -l $length -o ./xxx.img

    cd xxx/
    zcat ../xxx.img | cpio -idmu
    ko=$(find -name dm-mod.ko*)
    vtlog "ko=$ko ..."
    insmod $ko

    cd ../
    rm -f xxx.img
    rm -rf xxx
}

vtdiskname=$(get_ventoy_disk_name)
vtlog "vtdiskname=$vtdiskname ..."
if [ "$vtdiskname" = "unknown" ]; then
    exit 0
fi

if grep -q 'device-mapper' /proc/devices; then
    vtlog "device-mapper module check ko"
else
    install_dm_mod_ko
fi

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

ln -s /dev/dm-0 /dev/root

PATH=$VTPATH_OLD
