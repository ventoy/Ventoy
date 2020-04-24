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

porteus_hook() {
    $SED "/searching *for *\$SGN *file/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/porteus-disk.sh"  -i $1
    $SED "/searching *for *\$CFG *file/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/porteus-disk.sh"  -i $1
}

if $GREP -q exfat /proc/filesystems; then
    vtPath=$($VTOY_PATH/tool/vtoydump -p $VTOY_PATH/ventoy_os_param)
    
    $GREP '`value from`' /usr/* -r | $AWK -F: '{print $1}' | while read vtline; do
        echo "hooking $vtline ..." >> $VTLOG
        $SED "s#\`value from\`#$vtPath#g"  -i $vtline
    done

else
    for vtfile in '/init' '/linuxrc' ; do
        if [ -e $vtfile ]; then
            if ! $GREP -q ventoy $vtfile; then
                echo "hooking $vtfile ..."  >> $VTLOG
                porteus_hook $vtfile
            fi
        fi
    done
fi


# replace blkid in system
vtblkid=$($BUSYBOX_PATH/which blkid)
$BUSYBOX_PATH/rm -f $vtblkid
$BUSYBOX_PATH/cp -a $BUSYBOX_PATH/blkid $vtblkid
