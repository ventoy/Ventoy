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
    cp -a $VTOY_PATH/hook/rhel7/ventoy-autoexp.sh /lib/dracut/hooks/pre-mount/99-ventoy-autoexp.sh
else
    for vtParam in $($CAT /proc/cmdline); do
        if echo $vtParam | $GREP -q 'ks=file:/'; then
            continue
        fi
    
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
        $SED "/mount.*devspec.*\/run\/initramfs\/overlayfs/a . /ventoy/hook/rhel7/ventoy-overlay.sh" -i /sbin/dmsquash-live-root        
        $SED "s/osmin.img/osmin.imgxxxx/g" -i /sbin/dmsquash-live-root
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



vtNeedRepo=
if [ -f /etc/system-release ]; then
    if $GREP -q 'RED OS' /etc/system-release; then
        vtNeedRepo="yes"
    fi
fi

if $GREP -q 'el[89]' /proc/version; then
    vtNeedRepo="yes"
fi

if $GREP -i -q Fedora /proc/version; then
    if $GREP -q 'Server Edition' /etc/os-release; then
        vtNeedRepo="yes"
    fi
fi

if $GREP -i -q Fedora /etc/os-release; then
    if $GREP -q 'Server Edition' /etc/os-release; then
        vtNeedRepo="yes"
    fi
fi

echo "vtNeedRepo=$vtNeedRepo" >> $VTLOG

if [ "$vtNeedRepo" = "yes" ]; then
    $BUSYBOX_PATH/cp -a $VTOY_PATH/hook/rhel7/ventoy-repo.sh /lib/dracut/hooks/pre-pivot/99-ventoy-repo.sh
fi


#iso-scan (currently only for Fedora)
if $GREP -q Fedora /etc/os-release; then
if /ventoy/tool/vtoydump -a /ventoy/ventoy_os_param; then
    if ventoy_iso_scan_check; then
        echo "iso_scan process ..." >> $VTLOG
        
        vtIsoPath=$(/ventoy/tool/vtoydump -p /ventoy/ventoy_os_param)
        VTISO_SCAN="iso-scan/filename=$vtIsoPath"    
        echo -n $vtIsoPath > /ventoy/vtoy_iso_scan

        $SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE $VTISO_SCAN $VTKS $VTOVERLAY $vtInstDD#" -i /lib/dracut-lib.sh    
        if [ "$VTOY_LINUX_REMOUNT" = "01" -a "$vtNeedRepo" != "yes" ]; then
            ventoy_rw_iso_scan
        fi

        exit 0
    fi
fi    
fi


echo "common process ..." >> $VTLOG
if $GREP -q 'root=live' /proc/cmdline; then
    $SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE root=live:/dev/ventoy $VTKS $VTOVERLAY $VTISO_SCAN $vtInstDD#" -i /lib/dracut-lib.sh
else
    $SED "s#printf\(.*\)\$CMDLINE#printf\1\$CMDLINE inst.stage2=hd:/dev/ventoy $VTKS $VTOVERLAY $VTISO_SCAN $vtInstDD#" -i /lib/dracut-lib.sh
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

#For Fedora CoreOS
if $GREP -i -q 'fedora.*coreos' /etc/os-release; then
    $SED "s#isosrc=.*#isosrc=/dev/mapper/ventoy#" -i /lib/systemd/system-generators/live-generator
    cp -a $VTOY_PATH/hook/rhel7/ventoy-make-link.sh /lib/dracut/hooks/pre-mount/99-ventoy-premount-mklink.sh
fi


#special distro magic
$BUSYBOX_PATH/mkdir -p $VTOY_PATH/distmagic
if $GREP -q SCRE /proc/cmdline; then
    echo 1 > $VTOY_PATH/distmagic/SCRE
fi

if $GREP -qw 'SA[.]1' /proc/cmdline; then
if $GREP -qw 'writable.fsimg' /proc/cmdline; then
if $GREP -qw 'rw'     /proc/cmdline; then
    echo 1 > $VTOY_PATH/distmagic/DELL_PER
fi
fi
fi

