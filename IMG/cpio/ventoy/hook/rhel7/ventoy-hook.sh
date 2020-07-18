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

#ventoy_systemd_udevd_work_around
#ventoy_add_udev_rule "$VTOY_PATH/hook/default/udev_disk_hook.sh %k noreplace"

if [ -f $VTOY_PATH/autoinstall ]; then
    VTKS="inst.ks=file:$VTOY_PATH/autoinstall"
else
    for vtParam in $($CAT /proc/cmdline); do
        if echo $vtParam | $GREP -q 'inst.ks=hd:LABEL='; then
            vtRawKs=$(echo $vtParam | $AWK -F: '{print $NF}')
            VTKS="inst.ks=hd:/dev/dm-0:$vtRawKs"
            break
        fi
        
        if echo $vtParam | $GREP -q '^ks=.*:/'; then
            vtRawKs=$(echo $vtParam | $AWK -F: '{print $NF}')
            VTKS="ks=hd:/dev/dm-0:$vtRawKs"
            break
        fi
    done
fi

echo "VTKS=$VTKS" >> $VTLOG

if $GREP -q 'root=live' /proc/cmdline; then
    $SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE root=live:/dev/dm-0 $VTKS#" -i /lib/dracut-lib.sh
else
    $SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE inst.stage2=hd:/dev/dm-0 $VTKS#" -i /lib/dracut-lib.sh
fi

ventoy_set_inotify_script  rhel7/ventoy-inotifyd-hook.sh

#Fedora
if $BUSYBOX_PATH/which dmsquash-live-root > /dev/null; then
    vtPriority=99
else
    vtPriority=01
fi

$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/rhel7/ventoy-inotifyd-start.sh /lib/dracut/hooks/pre-udev/${vtPriority}-ventoy-inotifyd-start.sh
$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/rhel7/ventoy-timeout.sh /lib/dracut/hooks/initqueue/timeout/${vtPriority}-ventoy-timeout.sh

if [ -e /sbin/dmsquash-live-root ]; then
    echo "patch /sbin/dmsquash-live-root ..." >> $VTLOG
    $SED "1 a $BUSYBOX_PATH/sh $VTOY_PATH/hook/rhel7/ventoy-make-link.sh" -i /sbin/dmsquash-live-root
fi

# suppress write protected mount warning
if [ -e /usr/sbin/anaconda-diskroot ]; then
    $SED  's/^mount $dev $repodir/mount -oro $dev $repodir/' -i /usr/sbin/anaconda-diskroot
fi

