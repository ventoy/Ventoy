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

vtPath=$($VTOY_PATH/tool/vtoydump -p $VTOY_PATH/ventoy_os_param)
echo $vtPath | $GREP -q " "
_vtRet1=$?

$GREP -q exfat /proc/filesystems
_vtRet2=$?

echo "_vtRet1=$_vtRet1  _vtRet2=$_vtRet2 ..." >> $VTLOG

if [ $_vtRet1 -ne 0 -a $_vtRet2 -eq 0 ]; then
    vtFindFlag=0
    $GREP '`value from`' /usr/* -r | $AWK -F: '{print $1}' | while read vtline; do
        echo "hooking $vtline ..." >> $VTLOG
        $SED "s#\`value from\`#$vtPath#g"  -i $vtline
        vtFindFlag=1
    done

    if [ $vtFindFlag -eq 0 ]; then
        if $GREP -q '`value from`' /linuxrc; then
            if $GREP -q "searching *for *\$CFG *file" /linuxrc; then
                echo "hooking linuxrc CFG..." >> $VTLOG
                $SED "/searching *for *\$CFG *file/i$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/porteus-path.sh"  -i /linuxrc
                $SED "/searching *for *\$CFG *file/iFROM=\$(cat /porteus-from)"  -i /linuxrc
                $SED "/searching *for *\$CFG *file/iISO=\$(cat /porteus-from)"  -i /linuxrc
                vtFindFlag=1
            else
                echo "hooking linuxrc SGN..." >> $VTLOG
                $SED "/searching *for *\$SGN *file/i$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/porteus-path.sh"  -i /linuxrc
                $SED "/searching *for *\$SGN *file/iFROM=\$(cat /porteus-from)"  -i /linuxrc
                $SED "/searching *for *\$SGN *file/iISO=\$(cat /porteus-from)"  -i /linuxrc
                vtFindFlag=1
            fi
        fi
    fi

else
    for vtfile in '/linuxrc' '/init'; do
        if [ -e $vtfile ]; then
            if ! $GREP -q ventoy $vtfile; then
                echo "hooking disk $vtfile ..."  >> $VTLOG
                porteus_hook $vtfile
            fi
        fi
    done
fi


# replace blkid in system
vtblkid=$($BUSYBOX_PATH/which blkid)
$BUSYBOX_PATH/rm -f $vtblkid
$BUSYBOX_PATH/cp -a $BUSYBOX_PATH/blkid $vtblkid
