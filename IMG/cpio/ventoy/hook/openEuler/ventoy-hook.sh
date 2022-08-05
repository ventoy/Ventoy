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

if [ -f $VTOY_PATH/autoinstall ]; then
    VTKS="inst.ks=file:$VTOY_PATH/autoinstall"
else
    for vtParam in $($CAT /proc/cmdline); do
        if echo $vtParam | $GREP -q 'inst.ks=hd:LABEL='; then
            vtRawKsFull="$vtParam"
            vtRawKs=$(echo $vtParam | $AWK -F: '{print $NF}')
            VTKS="inst.ks=hd:/dev/ventoy:$vtRawKs"
            break
        fi
        
        if echo $vtParam | $GREP -q '^ks=.*:/'; then
            vtRawKsFull="$vtParam"
            vtRawKs=$(echo $vtParam | $AWK -F: '{print $NF}')
            VTKS="ks=hd:/dev/ventoy:$vtRawKs"
            break
        fi
        
        if echo $vtParam | $GREP -q '^inst.ks=.*:/'; then
            vtRawKsFull="$vtParam"
            vtRawKs=$(echo $vtParam | $AWK -F: '{print $NF}')
            VTKS="inst.ks=hd:/dev/ventoy:$vtRawKs"
            break
        fi
    done
fi

if [ -f $VTOY_PATH/ventoy_persistent_map ]; then
    VTOVERLAY="rd.live.overlay=/dev/dm-1:/vtoyoverlayfs/overlayfs"
    
    if [ -e /sbin/dmsquash-live-root ]; then
        echo "patch /sbin/dmsquash-live-root for persistent ..." >> $VTLOG
        $SED "/mount.*devspec.*\/run\/initramfs\/overlayfs/a . /ventoy/hook/openEuler/ventoy-overlay.sh" -i /sbin/dmsquash-live-root
    fi
    
    #close selinux
    $BUSYBOX_PATH/mkdir -p $VTOY_PATH/selinuxfs
    if $BUSYBOX_PATH/mount -t selinuxfs selinuxfs $VTOY_PATH/selinuxfs; then
        echo 1 > $VTOY_PATH/selinuxfs/disable
        $BUSYBOX_PATH/umount $VTOY_PATH/selinuxfs
    fi    
    $BUSYBOX_PATH/rm -rf $VTOY_PATH/selinuxfs
fi

echo "VTKS=$VTKS  VTOVERLAY=$VTOVERLAY" >> $VTLOG

if [ -n "$vtRawKs" ]; then
    if echo $vtRawKsFull | $EGREP -q "=http|=https|=ftp|=nfs|=hmc"; then
        echo "vtRawKsFull=$vtRawKsFull no patch needed." >> $VTLOG
        vtRawKs=""
        VTKS=""
    else
        echo "$vtRawKs" > $VTOY_PATH/ventoy_ks_rootpath
    fi    
fi

if ls $VTOY_PATH | $GREP -q 'ventoy_dud[0-9]'; then
    for vtDud in $(ls $VTOY_PATH/ventoy_dud*); do
        vtInstDD="$vtInstDD inst.dd=file:$vtDud"
    done
fi
echo "vtInstDD=$vtInstDD" >> $VTLOG

$SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE inst.stage2=hd:/dev/ventoy $VTKS $VTOVERLAY $vtInstDD#" -i /lib/dracut-lib.sh

ventoy_set_inotify_script  openEuler/ventoy-inotifyd-hook.sh

#Fedora
if $BUSYBOX_PATH/which dmsquash-live-root > /dev/null; then
    vtPriority=99
else
    vtPriority=01
fi

$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/openEuler/ventoy-inotifyd-start.sh /lib/dracut/hooks/pre-udev/${vtPriority}-ventoy-inotifyd-start.sh
$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/openEuler/ventoy-timeout.sh /lib/dracut/hooks/initqueue/timeout/${vtPriority}-ventoy-timeout.sh
$BUSYBOX_PATH/cp -a $VTOY_PATH/hook/openEuler/ventoy-repo.sh /lib/dracut/hooks/pre-pivot/99-ventoy-repo.sh

if [ -f /sbin/dmsquash-live-root ]; then
    echo "patch /sbin/dmsquash-live-root ..." >> $VTLOG
    $SED "1 a $BUSYBOX_PATH/sh $VTOY_PATH/hook/openEuler/ventoy-make-link.sh" -i /sbin/dmsquash-live-root
fi

# suppress write protected mount warning
if [ -f /usr/sbin/anaconda-diskroot ]; then
    $SED  's/^mount $dev $repodir/mount -oro $dev $repodir/' -i /usr/sbin/anaconda-diskroot
fi


if [ -f $VTOY_PATH/autoinstall ]; then
    cp -a $VTOY_PATH/hook/openEuler/ventoy-autoexp.sh /lib/dracut/hooks/pre-mount/99-ventoy-autoexp.sh
fi
