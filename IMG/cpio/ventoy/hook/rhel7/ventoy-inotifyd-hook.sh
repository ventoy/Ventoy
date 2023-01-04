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

if is_ventoy_hook_finished; then
    exit 0
fi

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

if is_inotify_ventoy_part $3; then

    vtlog "##### INOTIFYD: $2/$3 is created (YES) ..."

    

    vtlog "find ventoy partition ..."
    
    vtReplaceOpt=noreplace
    if [ -f /lib/dracut/hooks/pre-pivot/99-ventoy-repo.sh ]; then
        vtReplaceOpt=""
    fi
    
    $BUSYBOX_PATH/sh $VTOY_PATH/hook/default/udev_disk_hook.sh $3 $vtReplaceOpt
    
    blkdev_num_mknod=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    $BUSYBOX_PATH/mknod -m 660 /dev/ventoy  b  $blkdev_num_mknod
    $BUSYBOX_PATH/modprobe isofs >/dev/null 2>&1
    vtlog "mknod /dev/ventoy $blkdev_num_mknod"

    vtGenRulFile='/etc/udev/rules.d/99-live-squash.rules'
    if [ -e $vtGenRulFile ] && $GREP -q dmsquash $vtGenRulFile; then
        vtScript=$($GREP -m1 'RUN.=' $vtGenRulFile | $AWK -F'RUN.=' '{print $2}' | $SED 's/"\(.*\)".*/\1/')
        vtlog "vtScript=$vtScript"
        
        if [ -f $VTOY_PATH/distmagic/SCRE ]; then
            /sbin/dmsquash-live-root /dev/ventoy
        elif [ -f $VTOY_PATH/distmagic/DELL_PER ]; then
            sed 's/liverw=[^ ]*/liverw=ro/g' -i /sbin/dmsquash-live-root
            sed 's/writable_fsimg=[^ ]*/writable_fsimg=""/g' -i /sbin/dmsquash-live-root
            /sbin/dmsquash-live-root /dev/ventoy
        else
            $vtScript
        fi
    else
        vtlog "$vtGenRulFile not exist..."
    fi
    
    if [ -f $VTOY_PATH/ventoy_ks_rootpath ]; then
        vt_ks_rootpath=$(cat $VTOY_PATH/ventoy_ks_rootpath)
        vtlog "ks rootpath <$vt_ks_rootpath>"
        if [ -e /sbin/fetch-kickstart-disk ]; then
            vtlog "fetch-kickstart-disk ..."        
            /sbin/fetch-kickstart-disk /dev/ventoy "$vt_ks_rootpath"
        fi
    fi
    
    if [ -e /sbin/anaconda-diskroot ]; then
        vtlog "set anaconda-diskroot ..."        

        #busybox cp doesn't support -t option (issue 1900)
        /bin/cp -a /bin/cp $BUSYBOX_PATH/cp
        /sbin/anaconda-diskroot /dev/ventoy
    fi
    
    set_ventoy_hook_finish
else
    vtlog "##### INOTIFYD: $2/$3 is created (NO) ..."
fi

PATH=$VTPATH_OLD
