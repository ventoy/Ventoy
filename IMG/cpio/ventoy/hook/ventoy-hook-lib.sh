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

VTOY_PATH=/ventoy
BUSYBOX_PATH=$VTOY_PATH/busybox
VTLOG=$VTOY_PATH/log
FIND=$BUSYBOX_PATH/find
GREP=$BUSYBOX_PATH/grep
EGREP=$BUSYBOX_PATH/egrep
CAT=$BUSYBOX_PATH/cat
AWK=$BUSYBOX_PATH/awk
SED=$BUSYBOX_PATH/sed
SLEEP=$BUSYBOX_PATH/sleep
HEAD=$BUSYBOX_PATH/head
VTOY_DM_PATH=/dev/mapper/ventoy
VTOY_DEBUG_LEVEL=$($BUSYBOX_PATH/hexdump -n 1 -s 450 -e '1/1 "%02x"' $VTOY_PATH/ventoy_os_param)
VTOY_LINUX_REMOUNT=$($BUSYBOX_PATH/hexdump -n 1 -s 454 -e '1/1 "%02x"' $VTOY_PATH/ventoy_os_param)
VTOY_VLNK_BOOT=$($BUSYBOX_PATH/hexdump -n 1 -s 455 -e '1/1 "%02x"' $VTOY_PATH/ventoy_os_param)
if [ -d /var/lib/dracut/hooks ]; then
    VT_DRACUT_HOOKS=/var/lib/dracut/hooks
else
    VT_DRACUT_HOOKS=/lib/dracut/hooks
fi

if [ "$VTOY_DEBUG_LEVEL" = "01" ]; then
    if [ -e /dev/console ]; then
        VTLOG=/dev/console
    fi
fi


vtlog() {
    if [ "$VTLOG" = "$VTOY_PATH/log" ]; then
        echo "$*" >>$VTLOG
    else
        echo -e "\033[32m $* \033[0m" > $VTLOG
        $SLEEP 2
    fi
}

vterr() {
    if [ "$VTLOG" = "$VTOY_PATH/log" ]; then
        echo "$*" >>$VTLOG
    else
        echo -e "\n\033[31m $* \033[0m" > $VTLOG
        $SLEEP 30
    fi
}


ventoy_copy_file() {
    cp -a "$1" "$2"
    if [ -n "$3" ]; then
        chmod $3 "$2"
    fi
}


is_ventoy_hook_finished() {
    [ -e $VTOY_PATH/hook_finish ]
}

set_ventoy_hook_finish() {
    echo 'Y' > $VTOY_PATH/hook_finish

    if [ -f /ventoy/ventoy_iso_part_dm_cmd ]; then
        echo "### create iso part raw dm" >> $VTLOG
        $CAT $VTOY_PATH/ventoy_raw_table >> $VTLOG
        echo "### iso part dm cmd" >> $VTLOG
        $CAT /ventoy/ventoy_iso_part_dm_cmd >> $VTLOG        
        $BUSYBOX_PATH/sh /ventoy/ventoy_iso_part_dm_cmd >>$VTLOG 2>&1        
        $BUSYBOX_PATH/mv /ventoy/ventoy_iso_part_dm_cmd  /ventoy/ventoy_iso_part_dm_cmd_bk
    fi
}

get_ventoy_disk_name() {
    if [ "$VTOY_VLNK_BOOT" = "01" ]; then
        $VTOY_PATH/tool/vtoydump -t /ventoy/ventoy_os_param
    else
        line=$($VTOY_PATH/tool/vtoydump -f /ventoy/ventoy_os_param)
        if [ $? -eq 0 ]; then
            echo ${line%%#*}
        else    
            echo "unknown"
        fi
    fi
}

get_ventoy_iso_name() {
    line=$($VTOY_PATH/tool/vtoydump -f /ventoy/ventoy_os_param)
    if [ $? -eq 0 ]; then
        echo ${line##*#}
    else    
        echo "unknown"
    fi
}

wait_for_usb_disk_ready() {
    vtloop=0
    while [ -n "Y" ]; do
        usb_disk=$(get_ventoy_disk_name)
        vtlog "wait_for_usb_disk_ready $usb_disk ..."
        
        if echo $usb_disk | $EGREP -q "nvme|mmc|nbd"; then
            vtpart2=${usb_disk}p2
        else
            vtpart2=${usb_disk}2
        fi
        
        if [ -e "${vtpart2}" ]; then
            vtlog "wait_for_usb_disk_ready $usb_disk finish"
            break
        else
            let vtloop=vtloop+1
            if [ $vtloop -gt 2 ]; then
                if [ "$VTLOG" != "$VTOY_PATH/log" ]; then
                    $VTOY_PATH/tool/vtoydump -f /ventoy/ventoy_os_param -v > $VTLOG
                fi
            fi
            $SLEEP 0.3
        fi
    done
}


check_usb_disk_ready() {
    if echo $1 | $EGREP -q "nvme|mmc|nbd"; then
        vtpart2=${1}p2
    else
        vtpart2=${1}2
    fi
    
    [ -e "${vtpart2}" ]
}

not_ventoy_disk() {
    if echo $1 | $EGREP -q "nvme.*p$|mmc.*p$|nbd.*p$"; then
        vtDiskName=${1:0:-1}
    else
        vtDiskName=$1
    fi

    if [ "$VTOY_VLNK_BOOT" = "01" ]; then
        vtVtoyDisk=$($VTOY_PATH/tool/vtoydump -t $VTOY_PATH/ventoy_os_param)
        [ "$vtVtoyDisk" != "/dev/$vtDiskName" ]
    else
        if $VTOY_PATH/tool/vtoydump -f $VTOY_PATH/ventoy_os_param -c "$vtDiskName"; then
            $BUSYBOX_PATH/false
        else
            $BUSYBOX_PATH/true
        fi
    fi
}

ventoy_get_vblade_bin() {
    if $VTOY_PATH/tool/vblade_64 -t >>$VTLOG 2>&1; then
        echo $VTOY_PATH/tool/vblade_64
    else
        echo $VTOY_PATH/tool/vblade_32
    fi
}

ventoy_find_bin_path() {        
    #must use built-in dmsetup, so first try ventoy path. Otherwise ubuntu installer unhappy. #3567
    if $BUSYBOX_PATH/which "$1" > /dev/null; then
        $BUSYBOX_PATH/which "$1"; return
    fi
    for vt_path in '/bin' '/sbin' '/usr/bin' '/usr/sbin' '/usr/local/bin' '/usr/local/sbin' '/root/bin'; do
        if [ -e "$vt_path/$1" ]; then
            echo "$vt_path/$1"; return
        fi
    done

    echo ""
}


ventoy_find_bin_run() {    
    vtsudo=0
    if [ "$1" = "sudo" ]; then
        shift
        vtsudo=1
    fi
    
    vtbinpath=$(ventoy_find_bin_path "$1")
    if [ -n "$vtbinpath" ]; then
        shift
        
        if [ $vtsudo -eq 0 ]; then
            vtlog "$vtbinpath $*"
            $vtbinpath $*
        else
            vtlog "sudo $vtbinpath $*"
            sudo $vtbinpath $*
        fi
    fi
}

ventoy_get_module_postfix() {
    vtKerVer=$($BUSYBOX_PATH/uname -r)
    vtLine=$($FIND /lib/modules/$vtKerVer/ -name *.ko* | $HEAD -n1)
    vtComp=${vtLine##*/*.ko}
    echo "ko$vtComp"
}

ventoy_check_dm_module() {
    if $GREP -q 'device-mapper' /proc/devices; then
        $BUSYBOX_PATH/true; return
    fi
    
    vtlog "device-mapper NOT found in /proc/devices, try to load kernel module"
    $BUSYBOX_PATH/modprobe dm_mod >>$VTLOG 2>&1
    $BUSYBOX_PATH/modprobe dm-mod >>$VTLOG 2>&1
    
    if ! $GREP -q 'device-mapper' /proc/devices; then
        vtlog "modprobe failed, now try to insmod ko..."
    
        $FIND /lib/modules/ -name "dm-mod.ko*" | while read vtline; do
            vtlog "insmod $vtline "
            $BUSYBOX_PATH/insmod $vtline >>$VTLOG 2>&1
            if [ $? -eq 0 ]; then
                vtlog "insmod success"
            else
                vtlog "insmod failed, try decompress"
                if echo $vtline | $GREP -q "\.zst"; then
                    $VTOY_PATH/tool/zstdcat $vtline > $VTOY_PATH/extract_dm_mod.ko
                    $BUSYBOX_PATH/insmod $VTOY_PATH/extract_dm_mod.ko >>$VTLOG 2>&1
                fi
            fi
        done
    fi

    if $GREP -q 'device-mapper' /proc/devices; then
        vtlog "device-mapper found in /proc/devices after retry"
        $BUSYBOX_PATH/true; return
    else
        vtlog "device-mapper still NOT found in /proc/devices after retry"
        $BUSYBOX_PATH/false; return
    fi
}


create_ventoy_device_mapper() {
    vtlog "create_ventoy_device_mapper $*"
    
    VT_DM_BIN=$(ventoy_find_bin_path dmsetup)
    if [ -z "$VT_DM_BIN" ]; then
        vtlog "no dmsetup avaliable, lastly try inbox dmsetup"
        VT_DM_BIN=$VTOY_PATH/tool/dmsetup
    fi
    
    vtlog "dmsetup avaliable in system $VT_DM_BIN"

    if ventoy_check_dm_module "$1"; then
        vtlog "device-mapper module check success"
    else
        vterr "Error: no dm module avaliable"
    fi
    
    $VTOY_PATH/tool/vtoydm -p -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/ventoy_dm_table
    $VTOY_PATH/tool/vtoydm -r -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/ventoy_raw_table    

    if [ -z "$2" ]; then
        $VT_DM_BIN create ventoy $VTOY_PATH/ventoy_dm_table >>$VTLOG 2>&1
    else
        $VT_DM_BIN "$2" create ventoy $VTOY_PATH/ventoy_dm_table >>$VTLOG 2>&1
    fi

    RAWDISKNAME=$($HEAD -n1 $VTOY_PATH/ventoy_raw_table | $AWK '{print $4}')    
    echo "$VT_DM_BIN create  VentoyPart  $VTOY_PATH/ventoy_raw_table"  >  /ventoy/ventoy_iso_part_dm_cmd
    echo "echo 1 > $VTOY_PATH/ventoy_raw_dm_done"                      >> /ventoy/ventoy_iso_part_dm_cmd
}

create_ventoy_loop_device_mapper() {
    vtLoop1=${1#/dev/}
    vtLoop2=${2#/dev/}
    
    vtlog "create_ventoy_loop_device_mapper $*"
    
    VT_DM_BIN=$(ventoy_find_bin_path dmsetup)
    if [ -z "$VT_DM_BIN" ]; then
        vtlog "no dmsetup avaliable, lastly try inbox dmsetup"
        VT_DM_BIN=$VTOY_PATH/tool/dmsetup
    fi
    
    vtlog "dmsetup avaliable in system $VT_DM_BIN"

    if ventoy_check_dm_module; then
        vtlog "device-mapper module check success"
    else
        vterr "Error: no dm module avaliable"
    fi

    vtLoop2Size=$(cat /sys/block/$vtLoop2/size)
    echo "0 65 linear $1 0" > $VTOY_PATH/ventoy_dm_table
    echo "65 $vtLoop2Size linear $2 0" >> $VTOY_PATH/ventoy_dm_table

    $VT_DM_BIN "--readonly" create ventoy $VTOY_PATH/ventoy_dm_table >>$VTLOG 2>&1
}

create_ventoy_wrapper_device_mapper() {
    vtlog "create_ventoy_wrapper_device_mapper $*"
    
    VT_DM_BIN=$(ventoy_find_bin_path dmsetup)
    if [ -z "$VT_DM_BIN" ]; then
        vtlog "no dmsetup avaliable, lastly try inbox dmsetup"
        VT_DM_BIN=$VTOY_PATH/tool/dmsetup
    fi
    
    vtlog "dmsetup avaliable in system $VT_DM_BIN"

    if ventoy_check_dm_module "$1"; then
        vtlog "device-mapper module check success"
    else
        vterr "Error: no dm module avaliable"
    fi
    
    $VTOY_PATH/tool/vtoydm -w -f $VTOY_PATH/ventoy_image_map -d $1 > $VTOY_PATH/ventoy_dm_table
    
    VT_LOOP_DEV=$(losetup -f)
    losetup $VT_LOOP_DEV /ventoy/dmwrapper.bin
    $SED "s#/dev/loop0#$VT_LOOP_DEV#" -i $VTOY_PATH/ventoy_dm_table


    if [ -z "$2" ]; then
        $VT_DM_BIN create ventoy $VTOY_PATH/ventoy_dm_table >>$VTLOG 2>&1
    else
        $VT_DM_BIN "$2" create ventoy $VTOY_PATH/ventoy_dm_table >>$VTLOG 2>&1
    fi    
}

create_persistent_device_mapper() {
    vtlog "create_persistent_device_mapper $*"
    
    VT_DM_BIN=$(ventoy_find_bin_path dmsetup)
    if [ -z "$VT_DM_BIN" ]; then
        vtlog "no dmsetup avaliable, lastly try inbox dmsetup"
        VT_DM_BIN=$VTOY_PATH/tool/dmsetup
    fi
    
    vtlog "dmsetup avaliable in system $VT_DM_BIN"
        
    if ventoy_check_dm_module "$1"; then
        vtlog "device-mapper module check success"
    else
        vterr "Error: no dm module avaliable"
    fi
    
    $VTOY_PATH/tool/vtoydm -p -f $VTOY_PATH/ventoy_persistent_map -d $1 > $VTOY_PATH/persistent_dm_table        
    $VT_DM_BIN create vtoy_persistent $VTOY_PATH/persistent_dm_table >>$VTLOG 2>&1    
}



wait_for_ventoy_dm_disk_label() {
    DM=$($BUSYBOX_PATH/readlink $VTOY_DM_PATH)    
    vtlog "wait_for_ventoy_dm_disk_label $DM ..."
    
    for i in 0 1 2 3 4 5 6 7 8 9; do
        vtlog "i=$i ####### ls /dev/disk/by-label/"
        ls -l /dev/disk/by-label/ >> $VTLOG
        
        if ls -l /dev/disk/by-label/ | $GREP -q "$DM"; then
            break
        else
            $SLEEP 1
        fi
    done
}

install_udeb_pkg() {
    if ! [ -e "$1" ]; then
        $BUSYBOX_PATH/false
        return
    fi
    
    if [ -d /tmp/vtoy_udeb ]; then
        $BUSYBOX_PATH/rm -rf /tmp/vtoy_udeb
    fi
    
    $BUSYBOX_PATH/mkdir -p /tmp/vtoy_udeb
    $BUSYBOX_PATH/cp -a "$1" /tmp/vtoy_udeb/
    
    CURDIR=$($BUSYBOX_PATH/pwd)
    cd /tmp/vtoy_udeb
    
    $BUSYBOX_PATH/ar x "$1"
    
    if [ -e 'data.tar.gz' ]; then
        $BUSYBOX_PATH/tar -xzf data.tar.gz -C /
    elif [ -e 'data.tar.xz' ]; then
        $BUSYBOX_PATH/tar -xJf data.tar.xz -C /
    elif [ -e 'data.tar.bz2' ]; then
        $BUSYBOX_PATH/tar -xjf data.tar.bz2 -C /
    elif [ -e 'data.tar.lzma' ]; then
        $BUSYBOX_PATH/tar -xaf data.tar.lzma -C /
    fi
    
    if [ -e 'control.tar.gz' ]; then
        $BUSYBOX_PATH/tar -xzf control.tar.gz -C /
    elif [ -e 'control.tar.xz' ]; then
        $BUSYBOX_PATH/tar -xJf control.tar.xz -C /
    elif [ -e 'control.tar.bz2' ]; then
        $BUSYBOX_PATH/tar -xjf control.tar.bz2 -C /
    elif [ -e 'control.tar.lzma' ]; then
        $BUSYBOX_PATH/tar -xaf control.tar.lzma -C /
    fi
    
    cd $CURDIR
    $BUSYBOX_PATH/rm -rf /tmp/vtoy_udeb
    $BUSYBOX_PATH/true
}


install_udeb_from_line() {
    vtlog "install_udeb_from_line $1"

    if ! [ -b "$2" ]; then
        vterr "disk #$2# not exist"
        return 
    fi

    sector=$(echo $1 | $AWK '{print $(NF-1)}')
    length=$(echo $1 | $AWK '{print $NF}')
    vtlog "sector=$sector  length=$length"
    
    $VTOY_PATH/tool/vtoydm -e -f $VTOY_PATH/ventoy_image_map -d ${2} -s $sector -l $length -o /tmp/xxx.udeb
    if [ -e /tmp/xxx.udeb ]; then
        vtlog "extract udeb file from iso success"
    else
        vterr "extract udeb file from iso fail"
        return
    fi
    
    install_udeb_pkg /tmp/xxx.udeb
    $BUSYBOX_PATH/rm -f /tmp/xxx.udeb
}

extract_file_from_line() {
    vtlog "extract_file_from_line $1 disk=#$2#"
    if ! [ -b "$2" ]; then
        vterr "disk #$2# not exist"
        return 
    fi

    sector=$(echo $1 | $AWK '{print $(NF-1)}')
    length=$(echo $1 | $AWK '{print $NF}')
    vtlog "sector=$sector  length=$length"
    
    $VTOY_PATH/tool/vtoydm -e -f $VTOY_PATH/ventoy_image_map -d ${2} -s $sector -l $length -o $3
    if [ -e $3 ]; then
        vtlog "extract file from iso success"
        $BUSYBOX_PATH/true
    else
        vterr "extract file from iso fail"
        $BUSYBOX_PATH/false
    fi
}

extract_rpm_from_line() {
    vtlog "extract_rpm_from_line $1 disk=#$2#"

    if ! [ -b "$2" ]; then
        vterr "disk #$2# not exist"
        return 
    fi

    sector=$(echo $1 | $AWK '{print $(NF-1)}')
    length=$(echo $1 | $AWK '{print $NF}')
    vtlog "sector=$sector  length=$length"
    
    $VTOY_PATH/tool/vtoydm -e -f $VTOY_PATH/ventoy_image_map -d ${2} -s $sector -l $length -o /tmp/xxx.rpm
    if [ -e /tmp/xxx.rpm ]; then
        vtlog "extract rpm file from iso success"
    else
        vterr "extract rpm file from iso fail"
        return
    fi
    
    CURPWD=$($BUSYBOX_PATH/pwd)
    
    $BUSYBOX_PATH/mkdir -p $VTOY_PATH/rpm
    cd $VTOY_PATH/rpm
    vtlog "extract rpm..."
    $BUSYBOX_PATH/rpm2cpio /tmp/xxx.rpm | $BUSYBOX_PATH/cpio -idm 2>>$VTLOG
    cd $CURPWD
    
    $BUSYBOX_PATH/rm -f /tmp/xxx.rpm 
}

install_rpm_from_line() {
    vtlog "install_rpm_from_line $1 disk=#$2#"

    if ! [ -b "$2" ]; then
        vterr "disk #$2# not exist"
        return 
    fi

    sector=$(echo $1 | $AWK '{print $(NF-1)}')
    length=$(echo $1 | $AWK '{print $NF}')
    vtlog "sector=$sector  length=$length"
    
    $VTOY_PATH/tool/vtoydm -e -f $VTOY_PATH/ventoy_image_map -d ${2} -s $sector -l $length -o /tmp/xxx.rpm
    if [ -e /tmp/xxx.rpm ]; then
        vtlog "extract rpm file from iso success"
    else
        vterr "extract rpm file from iso fail"
        return
    fi
    
    CURPWD=$($BUSYBOX_PATH/pwd)
    
    cd /
    vtlog "install rpm..."
    $BUSYBOX_PATH/rpm2cpio /tmp/xxx.rpm | $BUSYBOX_PATH/cpio -idm 2>>$VTLOG
    cd $CURPWD
    
    $BUSYBOX_PATH/rm -f /tmp/xxx.rpm
}

dump_whole_iso_file() {
   $VTOY_PATH/tool/vtoydm -p -f $VTOY_PATH/ventoy_image_map -d $usb_disk | while read vtline; do
        vtlog "dmtable line: $vtline"
        vtcount=$(echo $vtline | $AWK '{print $2}')
        vtoffset=$(echo $vtline | $AWK '{print $NF}')
        $BUSYBOX_PATH/dd if=$usb_disk of="$1" bs=512 count=$vtcount skip=$vtoffset oflag=append conv=notrunc 
    done 
}

ventoy_copy_device_mapper() {
    if [ -L $VTOY_DM_PATH ]; then
        vtlog "replace block device link $1..."
        $BUSYBOX_PATH/mv "$1" $VTOY_PATH/dev_backup_${1#/dev/}
        VT_MAPPER_LINK=$($BUSYBOX_PATH/readlink $VTOY_DM_PATH)
        $BUSYBOX_PATH/cp -a "/dev/mapper/$VT_MAPPER_LINK" "$1"
    elif [ -b $VTOY_DM_PATH ]; then
        vtlog "replace block device $1..."
        $BUSYBOX_PATH/mv "$1" $VTOY_PATH/dev_backup_${1#/dev/}            
        $BUSYBOX_PATH/cp -a "$VTOY_DM_PATH" "$1"
    else
    
        vtlog "$VTOY_DM_PATH not exist, now check /dev/dm-X ..."
        VT_DM_BIN=$(ventoy_find_bin_path dmsetup)
        if [ -z "$VT_DM_BIN" ]; then
            vtlog "no dmsetup avaliable, lastly try inbox dmsetup"
            VT_DM_BIN=$VTOY_PATH/tool/dmsetup
        fi
    
        DM_VT_ID=$($VT_DM_BIN ls | $GREP ventoy | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
        vtlog "DM_VT_ID=$DM_VT_ID ..."
        $BUSYBOX_PATH/mv "$1" $VTOY_PATH/dev_backup_${1#/dev/}            
        $BUSYBOX_PATH/mknod -m 0666 "$1" b $DM_VT_ID
    fi 
}

# create link for device-mapper
ventoy_create_persistent_link() {
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | grep vtoy_persistent | sed 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1:\2/')  
    vtDM=$(ventoy_find_dm_id ${blkdev_num})

    if ! [ -d /dev/disk/by-label ]; then
        mkdir -p /dev/disk/by-label
    fi

    VTLABEL=$($BUSYBOX_PATH/blkid /dev/$vtDM | $SED 's/.*LABEL="\([^"]*\)".*/\1/')
    if [ -z "$VTLABEL" ]; then
        VTLABEL=casper-rw
    fi

    echo "$VTLABEL" > $VTOY_PATH/ventoy_persistent_label
    echo "/dev/$vtDM" > $VTOY_PATH/ventoy_persistent_dev

    vtlog "Persistent Label: ##${VTLABEL}##"

    if ! [ -e /dev/disk/by-label/$VTLABEL ]; then
        vtOldDir=$PWD
        cd /dev/disk/by-label
        ln -s ../../$vtDM $VTLABEL
        cd $vtOldDir
    fi    
}

ventoy_partname_to_diskname() {
    if echo $1 | $EGREP -q "nvme.*p[0-9]$|mmc.*p[0-9]$|nbd.*p[0-9]$"; then
        echo -n "${1:0:-2}"    
    else
        echo -n "${1:0:-1}"
    fi
}

ventoy_diskname_to_partname() {
    if echo $1 | $EGREP -q "nvme.*p[0-9]$|mmc.*p[0-9]$|nbd.*p[0-9]$"; then
        echo -n "${1}p$2"
    else
        echo -n "${1}$2"
    fi
}

ventoy_udev_disk_common_hook() {    
    if echo $1 | $EGREP -q "nvme.*p[0-9]$|mmc.*p[0-9]$|nbd.*p[0-9]$"; then
        VTDISK="${1:0:-2}"    
    else
        VTDISK="${1:0:-1}"
    fi
    
    if [ -e /vtoy/vtoy ]; then
        VTRWMOD=""
    else
        VTRWMOD="--readonly"
    fi
    
    # create device mapper for iso image file
    if create_ventoy_device_mapper "/dev/$VTDISK" $VTRWMOD; then
        vtlog "==== create ventoy device mapper success ===="
    else
        vtlog "==== create ventoy device mapper failed ===="
        
        $SLEEP 3
        
        if $GREP -q "/dev/$VTDISK" /proc/mounts; then
            $GREP "/dev/$VTDISK" /proc/mounts | while read vtLine; do
                vtPart=$(echo $vtLine | $AWK '{print $1}')
                vtMnt=$(echo $vtLine | $AWK '{print $2}')
                vtlog "$vtPart is mounted on $vtMnt  now umount it ..."
                $BUSYBOX_PATH/umount $vtMnt
            done
        fi
        
        if create_ventoy_device_mapper "/dev/$VTDISK" $VTRWMOD; then
            vtlog "==== create ventoy device mapper success after retry ===="
        else
            vtlog "==== create ventoy device mapper failed after retry ===="
            return
        fi
    fi
    
    if [ "$2" = "noreplace" ]; then
        vtlog "no need to replace block device"
    else
        ventoy_copy_device_mapper "/dev/$1"
    fi

    if [ -f $VTOY_PATH/ventoy_persistent_map ]; then
        create_persistent_device_mapper "/dev/$VTDISK"
        ventoy_create_persistent_link
    fi    
}

ventoy_create_wrapper_dm() {
    if echo $1 | $EGREP -q "nvme.*p[0-9]$|mmc.*p[0-9]$|nbd.*p[0-9]$"; then
        VTDISK="${1:0:-2}"    
    else
        VTDISK="${1:0:-1}"
    fi

    if [ -e /vtoy/vtoy ]; then
        VTRWMOD=""
    else
        VTRWMOD="--readonly"
    fi
    
    # create device mapper for iso image file
    if create_ventoy_wrapper_device_mapper "/dev/$VTDISK" $VTRWMOD; then
        vtlog "==== create wrapper ventoy device mapper success ===="
    else
        vtlog "==== create wrapper ventoy device mapper failed ===="
        
        $SLEEP 3
        
        if $GREP -q "/dev/$VTDISK" /proc/mounts; then
            $GREP "/dev/$VTDISK" /proc/mounts | while read vtLine; do
                vtPart=$(echo $vtLine | $AWK '{print $1}')
                vtMnt=$(echo $vtLine | $AWK '{print $2}')
                vtlog "$vtPart is mounted on $vtMnt  now umount it ..."
                $BUSYBOX_PATH/umount $vtMnt
            done
        fi
        
        if create_ventoy_wrapper_device_mapper "/dev/$VTDISK" $VTRWMOD; then
            vtlog "==== create wrapper ventoy device mapper success after retry ===="
        else
            vtlog "==== create wrapper ventoy device mapper failed after retry ===="
            return
        fi
    fi
    
    if [ "$2" = "noreplace" ]; then
        vtlog "no need to replace block device"
    else
        ventoy_copy_device_mapper "/dev/$1"
    fi
    
    if [ -f $VTOY_PATH/ventoy_persistent_map ]; then
        create_persistent_device_mapper "/dev/$VTDISK"
        ventoy_create_persistent_link
    fi
}


ventoy_create_dev_ventoy_part() {   
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    $BUSYBOX_PATH/mknod -m 0666 /dev/ventoy b $blkdev_num
    
    if [ -e /vtoy_dm_table ]; then
        vtPartid=1                        
        $CAT /vtoy_dm_table | while read vtline; do
            echo $vtline > /ventoy/dm_table_part${vtPartid}
            $VTOY_PATH/tool/dmsetup create ventoy${vtPartid} /ventoy/dm_table_part${vtPartid}
            
            blkdev_num=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy${vtPartid} | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
            $BUSYBOX_PATH/mknod -m 0666 /dev/ventoy${vtPartid} b $blkdev_num
            
            vtPartid=$(expr $vtPartid + 1)
        done   
    fi
}


ventoy_remove_all_dm() {
    if [ -e /vtoy_dm_table ]; then
        vtPartid=1                        
        $CAT /vtoy_dm_table | while read vtline; do            
            vtlog "dmsetup remove ventoy${vtPartid}"
            $VTOY_PATH/tool/dmsetup remove ventoy${vtPartid}
            vtPartid=$(expr $vtPartid + 1)
        done   
    fi

    vtlog "dmsetup remove ventoy"
    $VTOY_PATH/tool/dmsetup remove ventoy >> $VTLOG 2>&1
    rm -f /dev/ventoy*

    if [ -e $VTOY_PATH/ventoy_raw_dm_done ]; then
        vtlog "dmsetup remove VentoyPart"
        $VTOY_PATH/tool/dmsetup remove VentoyPart
        rm -f $VTOY_PATH/ventoy_iso_part_dm_cmd
    fi

    if [ -e $VTOY_PATH/persistent_dm_table ]; then
        if $VTOY_PATH/tool/dmsetup ls | $GREP -qw vtoy_persistent; then
            vtlog "dmsetup remove vtoy_persistent"
            $VTOY_PATH/tool/dmsetup remove vtoy_persistent
        else
            vtlog "no need to remove DM vtoy_persistent"
        fi
    fi
}


ventoy_create_chromeos_ventoy_part() {   
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    $BUSYBOX_PATH/mknod -m 0666 /dev/ventoy b $blkdev_num
    
    if [ -e /vtoy_dm_table ]; then
        vtPartid=1
        
        $CAT /vtoy_dm_table | while read vtline; do
            echo $vtline > /ventoy/dm_table_part${vtPartid}

            if [ $vtPartid -eq $1 ]; then
                $VTOY_PATH/tool/dmsetup create ventoy${vtPartid} /ventoy/dm_table_part${vtPartid} --readonly
            else
                $VTOY_PATH/tool/dmsetup create ventoy${vtPartid} /ventoy/dm_table_part${vtPartid}
            fi

            blkdev_num=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy${vtPartid} | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
            $BUSYBOX_PATH/mknod -m 0666 /dev/ventoy${vtPartid} b $blkdev_num
            
            vtPartid=$(expr $vtPartid + 1)
        done        
    fi
}

is_inotify_ventoy_part() {
    if echo $1 | $GREP -q "2$"; then
        if ! [ -e /sys/block/$1 ]; then
            if [ -e /sys/class/block/$1 ]; then
                if echo $1 | $EGREP -q "nvme|mmc|nbd"; then
                    vtShortName=${1:0:-2}
                else
                    vtShortName=${1:0:-1}
                fi
                
                if [ -e /dev/$vtShortName ]; then
                    if [ "$VTOY_VLNK_BOOT" = "01" ]; then
                        vtOrgDiskName=$($VTOY_PATH/tool/vtoydump -t $VTOY_PATH/ventoy_os_param)
                        [ "$vtOrgDiskName" = "/dev/$vtShortName" ]
                    else
                        $VTOY_PATH/tool/vtoydump -f $VTOY_PATH/ventoy_os_param -c $vtShortName
                    fi
                    return
                fi
            fi
        fi
    fi
    
    [ "1" = "0" ]
}

ventoy_find_dm_id() {
    for vt in $($BUSYBOX_PATH/ls /sys/block/); do
        if [ "${vt:0:3}" = "dm-" ]; then
            vtMajorMinor=$($CAT /sys/block/$vt/dev)
            if [ "$vtMajorMinor" = "$1" ]; then
                echo ${vt}
                return
            fi
        fi
    done
    echo 'xx'
}

ventoy_swap_device() {
    mv $1 $VTOY_PATH/swap_tmp_dev
    mv $2 $1
    mv $VTOY_PATH/swap_tmp_dev $2
}

ventoy_extract_vtloopex() {
    vtCurPwd=$PWD
    $BUSYBOX_PATH/mkdir -p $VTOY_PATH/partmnt $VTOY_PATH/vtloopex
    $BUSYBOX_PATH/mount -o ro -t vfat $1  $VTOY_PATH/partmnt
    cd $VTOY_PATH/vtloopex
    $CAT $VTOY_PATH/partmnt/ventoy/vtloopex.cpio | $BUSYBOX_PATH/cpio -idm >> $VTLOG 2>&1
    $BUSYBOX_PATH/umount $VTOY_PATH/partmnt
    $BUSYBOX_PATH/rm -rf $VTOY_PATH/partmnt    

    if [ -n "$2" ]; then
        cd $VTOY_PATH/vtloopex/$2/
        $BUSYBOX_PATH/tar -xJf vtloopex.tar.xz
    fi
    
    cd $vtCurPwd
}

ventoy_check_install_module_xz() {
    if [ -f "${1}.xz" ]; then
        $BUSYBOX_PATH/xz -d  "${1}.xz"
        $BUSYBOX_PATH/insmod "$1"
    fi
}

ventoy_check_umount() {
    for vtLoop in 0 1 2 3 4 5 6 7 8 9; do
        $BUSYBOX_PATH/umount "$1" > /dev/null 2>&1
        if $BUSYBOX_PATH/mountpoint -q "$1"; then
            $SLEEP 1
        else
            break
        fi
    done
}

ventoy_wait_dir() {
    vtdir=$1
    vtsec=0
    
    while [ $vtsec -lt $2 ]; do
        if [ -d "$vtdir" ]; then
            break
        else
            $SLEEP 1
            vtsec=$(expr $vtsec + 1)
        fi
    done
}

ventoy_load_iso_part_fs_ko() {
    vtFS=$($VTOY_PATH/tool/vtoydump -s /ventoy/ventoy_os_param)
    vtlog "ISO part FS is $vtFS"
    
    for vtModProbe in /usr/sbin/modprobe /sbin/modprobe /bin/modprobe $BUSYBOX_PATH/modprobe; do
        if [ -e $vtModProbe ]; then
            break
        fi
    done
    
    vtlog "modprobe is $vtModProbe"
    
    if [ "$vtFS" = "ext" ]; then    
        vtlog "modprobe ext2/3/4"
        $vtModProbe ext2 >> $VTLOG 2>&1
        $vtModProbe ext3 >> $VTLOG 2>&1
        $vtModProbe ext4 >> $VTLOG 2>&1
    elif [ "$vtFS" = "ntfs" ]; then
        vtlog "modprobe ntfs3"
        $vtModProbe ntfs3 >> $VTLOG 2>&1
    elif [ "$vtFS" = "fat" ]; then
        vtlog "modprobe vfat"
        $vtModProbe vfat   >> $VTLOG 2>&1      
    else
        vtlog "modprobe $vtFS"
        $vtModProbe $vtFS  >> $VTLOG 2>&1
    fi
}


ventoy_iso_part_fs_supported() {
    vtFS=$($VTOY_PATH/tool/vtoydump -s /ventoy/ventoy_os_param)    
    if [ "$vtFS" = "ext" ]; then
        vtPattern='ext[2-4]'
    elif [ "$vtFS" = "fat" ]; then
        vtPattern='vfat'
    elif [ "$vtFS" = "ntfs" ]; then
        vtPattern='ntfs3'
    else
        vtPattern="$vtFS"
    fi
    
    vtlog "vtFS=$vtFS Pattern=$vtPattern"
    if $EGREP -q "$vtPattern" /proc/filesystems; then
        vtlog "ISO part FS supported OK"
        $BUSYBOX_PATH/true
    else
        vtlog "ISO part FS is NOT supported"
        $BUSYBOX_PATH/false
    fi
}

ventoy_mount_iso_part() {
    vtDisk=$1
    vtMnt=$2

    vtlog "mount iso partition $vtDisk"

    if [ -z "$vtMnt" ]; then
        vtMnt=$VTOY_PATH/tmpmnt
    fi
    
    mkdir -p $vtMnt

    if echo $vtDisk | $EGREP -q "nvme|mmc|nbd"; then
        vtpart1=${vtDisk}p1
    else
        vtpart1=${vtDisk}1
    fi

    vtFS=$($VTOY_PATH/tool/vtoydump -s /ventoy/ventoy_os_param)    
    if [ "$vtFS" = "ntfs" ]; then
        vtlog "mount -t ntfs3 $vtpart1 $vtMnt"
        mount -t ntfs3 $vtpart1 $vtMnt >> $VTLOG 2>&1
    elif [ "$vtFS" = "fat" ]; then
        vtlog "mount -t vfat $vtpart1 $vtMnt"
        mount -t vfat $vtpart1 $vtMnt >> $VTLOG 2>&1
    else
        vtlog "mount $vtpart1 $vtMnt"
        mount $vtpart1 $vtMnt >> $VTLOG 2>&1
    fi
}


ventoy_losetup_iso() {
    vtMnt=$1
    if [ -z "$vtMnt" ]; then
        vtMnt=$VTOY_PATH/tmpmnt
    fi 

    for vtLosetup in /usr/sbin/losetup /sbin/losetup /bin/losetup $BUSYBOX_PATH/losetup; do
        if [ -e $vtLosetup ]; then
            break
        fi
    done

    vtisoname=$(get_ventoy_iso_name)    

    dd if=/dev/zero of=/ventoy/isoheader.bin bs=512 count=1 >/dev/null 2>&1
    dd if="${vtMnt}${vtisoname}" bs=512 skip=1 count=64 >>/ventoy/isoheader.bin  2>>/dev/null

    vtfreeloop1=$($vtLosetup -f)
    if $vtLosetup -r $vtfreeloop1 /ventoy/isoheader.bin >> $VTLOG 2>&1; then    
        vtfreeloop2=$($vtLosetup -f)
        if $vtLosetup -r --offset=33280 $vtfreeloop2 "${vtMnt}${vtisoname}" >> $VTLOG 2>&1; then
            echo -n "$vtfreeloop1 $vtfreeloop2" > /ventoy/vtoy_dm_loop_dev
        fi
    fi
}


ventoy_mount_iso() {
    if [ "$1" = "ko" ]; then
        shift
        ventoy_load_iso_part_fs_ko
    fi
    if ventoy_iso_part_fs_supported; then
        if ventoy_mount_iso_part $1; then
            if ventoy_losetup_iso; then
                create_ventoy_loop_device_mapper $($CAT /ventoy/vtoy_dm_loop_dev)
                return
            else
                vtlog "losetup ISO file FAILED"
            fi
        else
            vtlog "mount iso partition FAILED"
        fi
    fi

    $BUSYBOX_PATH/false
}

ventoy_init_udev_auto_rules() {
    ventoy_copy_file /ventoy/hook/default/11-ventoy-dm.rules  /etc/udev/rules.d/11-ventoy-dm.rules  0644
}

ventoy_new_root_dir() {
    for vroot in sysroot newroot new_root root live/aufs pup_new; do
        if [ -d $vroot/etc/udev/rules.d ]; then
            echo "$vroot"
            return
        fi
    done
    echo ''
}

ventoy_copy_udev_auto_rules() {
    if ! [ -e $VTOY_PATH/ventoy_raw_dm_done ]; then
        vtlog "ventoy_raw_dm_done not exist, no need for udev auto rule"
        return
    fi

    vtlog "==== Copy udev auto rules ===="
    vroot=$(ventoy_new_root_dir)
    if [ -z "$vroot" ]; then
        vtlog "no new root found"
        return
    else
        vtlog "new root is $vroot"
    fi

    
    cp -a /ventoy/hook/default/90-ventoy-auto.rules /ventoy/udevtmp.rules        
    vtDM=$($VTOY_PATH/tool/dmsetup info VentoyPart | $GREP Major | $SED "s/.*[^0-9]\([0-9][0-9]*\)$/\1/")
    $SED "s/DMXXX/dm-${vtDM}/g" -i /ventoy/udevtmp.rules

    vtRAWDISKNAME=$($HEAD -n1 $VTOY_PATH/ventoy_raw_table | $AWK '{print $4}')
    $SED "s/yyy/${vtRAWDISKNAME#/dev/}/g" -i /ventoy/udevtmp.rules

    vtlog "VentoyPart is /dev/dm-${vtDM} map to $vtRAWDISKNAME  NewRoot is $vroot"

    ventoy_copy_file /ventoy/udevtmp.rules  $vroot/etc/udev/rules.d/90-ventoy-auto.rules  0644
    ventoy_copy_file /ventoy/udevtmp.rules  $vroot/etc/udev/rules.d/11-ventoy-auto.rules  0644

    if $GREP -q 'init=.*sysvinit' /proc/cmdline; then
        vtlog "copy sysvinit udev trigger script"        
        cp -a /ventoy/hook/default/ventoy-udev-trigger.rc /ventoy/udevtmp.rc
        $SED "s/DMXXX/dm-${vtDM}/g" -i /ventoy/udevtmp.rc

        mkdir -p $vroot/etc/boot.d        
        ventoy_copy_file /ventoy/udevtmp.rc  $vroot/etc/boot.d/S01-ventoy-udev-trigger  0755
    elif [ -d $vroot/etc/systemd/system ]; then
        vtlog "copy systemd udev trigger service"
        
        mkdir -p $vroot/etc/systemd/system/sysinit.target.wants
        cp -a /ventoy/hook/default/ventoy-udev-trigger.service /ventoy/udevtmp.service
        $SED "s/DMXXX/dm-${vtDM}/g" -i /ventoy/udevtmp.service
        ventoy_copy_file /ventoy/udevtmp.service  $vroot/etc/systemd/system/ventoy-udev-trigger.service  0644
        
        mkdir -p $vroot/etc/systemd/system-preset
        echo 'enable ventoy-udev-trigger.service' > $vroot/etc/systemd/system-preset/80-ventoy-udev.preset

        $BUSYBOX_PATH/ln -sf /etc/systemd/system/ventoy-udev-trigger.service $vroot/etc/systemd/system/sysinit.target.wants/ventoy-udev-trigger.service
    else
        vtlog "sysinit wants dir not exist"
    fi
}

ventoy_dracut_pivot_udev_rule() {
    echo "#!/bin/sh" > $VT_DRACUT_HOOKS/pre-pivot/99-ventoy-udev-rules.sh
    echo "/ventoy/busybox/sh $VTOY_PATH/hook/default/ventoy-pivot-udev-rule.sh" >> $VT_DRACUT_HOOKS/pre-pivot/99-ventoy-udev-rules.sh
}

ventoy_dracut_pivot_selinux_off() {
    echo "#!/bin/sh" > $VT_DRACUT_HOOKS/pre-pivot/99-ventoy-selinux-off.sh
    echo "/ventoy/busybox/sh $VTOY_PATH/hook/default/ventoy-pivot-selinux-off.sh" >> $VT_DRACUT_HOOKS/pre-pivot/99-ventoy-selinux-off.sh
}

