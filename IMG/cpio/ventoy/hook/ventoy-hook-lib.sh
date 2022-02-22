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


is_ventoy_hook_finished() {
    [ -e $VTOY_PATH/hook_finish ]
}

set_ventoy_hook_finish() {
    echo 'Y' > $VTOY_PATH/hook_finish
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
            vtlog "insmode $vtline "
            $BUSYBOX_PATH/insmod $vtline >>$VTLOG 2>&1
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

ventoy_need_dm_patch() {
    if [ "$VTOY_LINUX_REMOUNT" != "01" ]; then
        $BUSYBOX_PATH/false; return
    fi

    if $GREP -q 'device-mapper' /proc/devices; then
        :
    else
        $BUSYBOX_PATH/false; return
    fi
    
    if $GREP -q 'dm_patch' /proc/modules; then
        $BUSYBOX_PATH/false; return
    fi
    
    vtMajorVer=$($BUSYBOX_PATH/uname -r | $AWK -F. '{print $1}')
    vtMinorVer=$($BUSYBOX_PATH/uname -r | $AWK -F. '{print $2}')

    if [ $vtMajorVer -lt 3 ]; then
        $BUSYBOX_PATH/false; return
    elif [ $vtMajorVer -eq 3 -a $vtMinorVer -lt 10 ]; then
        $BUSYBOX_PATH/false; return
    fi
    
    
    $BUSYBOX_PATH/true
}

ventoy_dm_patch() {
    vtMType=$($BUSYBOX_PATH/uname -m)

    vtlog "######### ventoy_dm_patch ############"

    if echo $vtMType | $EGREP -i -q "x86.64|amd64"; then
        vtKoName=dm_patch_64.ko
    elif echo $vtMType | $EGREP -i -q "i[3-6]86"; then
        vtKoName=dm_patch_32.ko
    else
        vtlog "unsupported machine type $vtMType"
        return
    fi

    if [ -f $VTOY_PATH/tool/$vtKoName ]; then
        vtlog "/ventoy/tool/$vtKoName exist OK"
    else
        vtlog "/ventoy/tool/$vtKoName NOT exist"
        return
    fi
    
    $CAT /proc/kallsyms | $BUSYBOX_PATH/sort > $VTOY_PATH/kallsyms
    
    vtLine=$($VTOY_PATH/tool/vtoyksym dm_get_table_device $VTOY_PATH/kallsyms)
    get_addr=$(echo $vtLine | $AWK '{print $1}')
    get_size=$(echo $vtLine | $AWK '{print $2}')

    vtLine=$($VTOY_PATH/tool/vtoyksym dm_put_table_device $VTOY_PATH/kallsyms)
    put_addr=$(echo $vtLine | $AWK '{print $1}')
    put_size=$(echo $vtLine | $AWK '{print $2}')
    
    ro_addr=$($GREP ' set_memory_ro$' /proc/kallsyms | $AWK '{print $1}')
    rw_addr=$($GREP ' set_memory_rw$' /proc/kallsyms | $AWK '{print $1}')
    kprobe_reg_addr=$($GREP ' register_kprobe$' /proc/kallsyms | $AWK '{print $1}')
    kprobe_unreg_addr=$($GREP ' unregister_kprobe$' /proc/kallsyms | $AWK '{print $1}')
    
    if [ "$VTOY_DEBUG_LEVEL" = "01" ]; then
        printk_addr=$($GREP ' printk$' /proc/kallsyms | $AWK '{print $1}')
        vtDebug="-v"
    else
        printk_addr=0
    fi
    
    #printk_addr=$($GREP ' printk$' /proc/kallsyms | $AWK '{print $1}')
    #vtDebug="-v"
    
    vtlog get_addr=$get_addr  get_size=$get_size
    vtlog put_addr=$put_addr  put_size=$put_size
    vtlog kprobe_reg_addr=$kprobe_reg_addr  kprobe_unreg_addr=$kprobe_unreg_addr
    vtlog ro_addr=$ro_addr  rw_addr=$rw_addr  printk_addr=$printk_addr

    if [ "$get_addr" = "0" -o "$put_addr" = "0" ]; then
        vtlog "Invalid symbol address"
        return
    fi
    if [ "$ro_addr" = "0" -o "$rw_addr" = "0" ]; then
        vtlog "Invalid symbol address"
        return
    fi


    vtKv=$($BUSYBOX_PATH/uname -r)
    
    if [ ! -d /lib/modules/$vtKv ]; then
        vtlog "No modules directory found"
        return
    elif [ -d /lib/modules/$vtKv/kernel/fs ]; then
        vtModPath=$($FIND /lib/modules/$vtKv/kernel/fs/ -name "*.ko*" | $HEAD -n1)
    else
        vtModPath=$($FIND /lib/modules/$vtKv/kernel/ -name "xfs.ko*" | $HEAD -n1)
    fi
    
    if [ -z "$vtModPath" ]; then
        vtModPath=$($FIND /lib/modules/$vtKv/kernel/ -name "*.ko*" | $HEAD -n1)
    fi
    
    vtModName=$($BUSYBOX_PATH/basename $vtModPath)
    
    vtlog "template module is $vtModPath $vtModName"
    
    if [ -z "$vtModPath" ]; then
        vtlog "No template module found"
        return
    elif echo $vtModPath | $GREP -q "[.]ko$"; then
        $BUSYBOX_PATH/cp -a $vtModPath  $VTOY_PATH/$vtModName
    elif echo $vtModPath | $GREP -q "[.]ko[.]xz$"; then
        $BUSYBOX_PATH/xzcat $vtModPath > $VTOY_PATH/$vtModName
    elif echo $vtModPath | $GREP -q "[.]ko[.]gz$"; then
        $BUSYBOX_PATH/zcat $vtModPath > $VTOY_PATH/$vtModName
    else
        vtlog "unsupport module type"
        return
    fi
    
    #step1: modify vermagic/mod crc/relocation
    $VTOY_PATH/tool/vtoykmod -u $VTOY_PATH/tool/$vtKoName $VTOY_PATH/$vtModName $vtDebug
    
    #step2: fill parameters
    vtPgsize=$($VTOY_PATH/tool/vtoyksym -p)
    $VTOY_PATH/tool/vtoykmod -f $VTOY_PATH/tool/$vtKoName $vtPgsize 0x$printk_addr 0x$ro_addr 0x$rw_addr $get_addr $get_size $put_addr $put_size 0x$kprobe_reg_addr 0x$kprobe_unreg_addr $vtDebug

    $BUSYBOX_PATH/insmod $VTOY_PATH/tool/$vtKoName
    
    if $GREP -q 'dm_patch' /proc/modules; then
        echo "done" > $VTOY_PATH/dm_patch_done
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
    
    
    vtLevel1=$($CAT /proc/sys/kernel/printk | $AWK '{print $1}')
    vtLevel2=$($CAT /proc/sys/kernel/printk | $AWK '{print $2}')
    vtLevel3=$($CAT /proc/sys/kernel/printk | $AWK '{print $3}')
    vtLevel4=$($CAT /proc/sys/kernel/printk | $AWK '{print $4}')
    if ventoy_need_dm_patch; then
        ventoy_dm_patch
        #suppress printk message
        echo 0 $vtLevel2 0 $vtLevel4 > /proc/sys/kernel/printk
    fi

    if [ -z "$2" ]; then
        $VT_DM_BIN create ventoy $VTOY_PATH/ventoy_dm_table >>$VTLOG 2>&1
    else
        $VT_DM_BIN "$2" create ventoy $VTOY_PATH/ventoy_dm_table >>$VTLOG 2>&1
    fi

    if ventoy_need_dm_patch; then    
        #recover printk level
        echo $vtLevel1 $vtLevel2 $vtLevel3 $vtLevel4 > /proc/sys/kernel/printk
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


    vtLevel1=$($CAT /proc/sys/kernel/printk | $AWK '{print $1}')
    vtLevel2=$($CAT /proc/sys/kernel/printk | $AWK '{print $2}')
    vtLevel3=$($CAT /proc/sys/kernel/printk | $AWK '{print $3}')
    vtLevel4=$($CAT /proc/sys/kernel/printk | $AWK '{print $4}')
    if [ -f $VTOY_PATH/dm_patch_done ]; then
        #suppress printk message
        echo 0 $vtLevel2 0 $vtLevel4 > /proc/sys/kernel/printk
    fi
    
    $VT_DM_BIN create vtoy_persistent $VTOY_PATH/persistent_dm_table >>$VTLOG 2>&1
    
    if [ -f $VTOY_PATH/dm_patch_done ]; then
        #recover printk level
        echo $vtLevel1 $vtLevel2 $vtLevel3 $vtLevel4 > /proc/sys/kernel/printk
    fi
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
    
    if $GREP -q 'dm_patch' /proc/modules; then
        $BUSYBOX_PATH/rmmod dm_patch
    fi
}

ventoy_create_dev_ventoy_part() {   
    blkdev_num=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
    $BUSYBOX_PATH/mknod -m 0666 /dev/ventoy b $blkdev_num
    
    if [ -e /vtoy_dm_table ]; then
        vtPartid=1
        
        vtLevel1=$($CAT /proc/sys/kernel/printk | $AWK '{print $1}')
        vtLevel2=$($CAT /proc/sys/kernel/printk | $AWK '{print $2}')
        vtLevel3=$($CAT /proc/sys/kernel/printk | $AWK '{print $3}')
        vtLevel4=$($CAT /proc/sys/kernel/printk | $AWK '{print $4}')
        if [ -f $VTOY_PATH/dm_patch_done ]; then
            #suppress printk message
            echo 0 $vtLevel2 0 $vtLevel4 > /proc/sys/kernel/printk
        fi
        
        $CAT /vtoy_dm_table | while read vtline; do
            echo $vtline > /ventoy/dm_table_part${vtPartid}
            $VTOY_PATH/tool/dmsetup create ventoy${vtPartid} /ventoy/dm_table_part${vtPartid}
            
            blkdev_num=$($VTOY_PATH/tool/dmsetup ls | $GREP ventoy${vtPartid} | $SED 's/.*(\([0-9][0-9]*\),.*\([0-9][0-9]*\).*/\1 \2/')
            $BUSYBOX_PATH/mknod -m 0666 /dev/ventoy${vtPartid} b $blkdev_num
            
            vtPartid=$(expr $vtPartid + 1)
        done   

        if [ -f $VTOY_PATH/dm_patch_done ]; then
            #recover printk level
            echo $vtLevel1 $vtLevel2 $vtLevel3 $vtLevel4 > /proc/sys/kernel/printk
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
