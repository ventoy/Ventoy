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

VT_RULE_DIR_PREFIX=""
VT_PRINTK_LEVEL=0
VT_UDEV_RULE_FILE_NAME="99-ventoy.rules"
VT_UDEV_RULE_PREFIX="ACTION==\"add\", SUBSYSTEM==\"block\","

ventoy_close_printk() {
    VT_PRINTK_LEVEL=$($CAT /proc/sys/kernel/printk | $AWK '{print $1}')
    if [ -e /proc/sys/kernel/printk ]; then
        echo 0 > /proc/sys/kernel/printk
    fi
}

ventoy_restore_printk() {
    if [ -e /proc/sys/kernel/printk ]; then
        echo $VT_PRINTK_LEVEL > /proc/sys/kernel/printk
    fi
}

ventoy_set_rule_dir_prefix() {
    VT_RULE_DIR_PREFIX=$1
}

ventoy_get_udev_conf_dir() {
    if [ -d $VT_RULE_DIR_PREFIX/etc/udev/rules.d ]; then
        VT_RULE_PATH=$VT_RULE_DIR_PREFIX/etc/udev/rules.d
    elif [ -d $VT_RULE_DIR_PREFIX/lib/udev/rules.d ]; then
        VT_RULE_PATH=$VT_RULE_DIR_PREFIX/lib/udev/rules.d
    else
        $BUSYBOX_PATH/mkdir -p $VT_RULE_DIR_PREFIX/etc/udev/rules.d
        VT_RULE_PATH=$VT_RULE_DIR_PREFIX/etc/udev/rules.d
    fi
    echo -n "$VT_RULE_PATH"
}

ventoy_get_udev_conf_path() {
    VT_RULE_DIR=$(ventoy_get_udev_conf_dir)
    echo "$VT_RULE_DIR/$VT_UDEV_RULE_FILE_NAME"
}

ventoy_add_kernel_udev_rule() {
    VT_UDEV_RULE_PATH=$(ventoy_get_udev_conf_path)
    echo "KERNEL==\"$1\", $VT_UDEV_RULE_PREFIX RUN+=\"$2\"" >> $VT_UDEV_RULE_PATH
}

ventoy_add_udev_rule_with_name() {
    VT_UDEV_RULE_DIR=$(ventoy_get_udev_conf_dir)
    echo "KERNEL==\"*2\", $VT_UDEV_RULE_PREFIX RUN+=\"$1\"" >> $VT_UDEV_RULE_DIR/$2
}

ventoy_add_udev_rule_with_path() {
    echo "KERNEL==\"*2\", $VT_UDEV_RULE_PREFIX RUN+=\"$1\"" >> $2
}

ventoy_add_udev_rule() {
    VT_UDEV_RULE_PATH=$(ventoy_get_udev_conf_path)
    echo "KERNEL==\"*2\", $VT_UDEV_RULE_PREFIX RUN+=\"$1\"" >> $VT_UDEV_RULE_PATH
}

#
# It seems there is a bug in somw version of systemd-udevd
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=869719
#
ventoy_systemd_udevd_work_around() {
    for vtdir in 'lib' 'usr/lib'; do
    
        VTSYSTEMUDEV="$VT_RULE_DIR_PREFIX/$vtdir/systemd/system/systemd-udevd.service"
        if [ -e $VTSYSTEMUDEV ]; then
            if $GREP -q 'SystemCallArchitectures.*native' $VTSYSTEMUDEV; then
                $SED "s/.*\(SystemCallArchitectures.*native\)/#\1/g"  -i $VTSYSTEMUDEV
                break
            fi
        fi
    done
}


ventoy_print_yum_repo() {
    echo "[$1]"
    echo "name=$1"
    echo "baseurl=$2"
    echo "enabled=1"
    echo "gpgcheck=0"
    echo "priority=0"
}

ventoy_set_inotify_script() {
    echo $VTOY_PATH/hook/$1 > $VTOY_PATH/inotifyd-hook-script.txt
}

ventoy_set_loop_inotify_script() {
    echo $VTOY_PATH/loop/$1 > $VTOY_PATH/inotifyd-loop-script.txt
}

ventoy_check_insmod() {
    if [ -e $1 ]; then
        $BUSYBOX_PATH/insmod $1
    fi
}

ventoy_check_mount() {
    if [ -e $1 ]; then
        $BUSYBOX_PATH/mount $1 $2
    fi
}

ventoy_has_exfat_ko() {
    vtExfat=''
    vtKerVer=$($BUSYBOX_PATH/uname -r)
    if [ -d /lib/modules/$vtKerVer/kernel/fs/exfat ]; then
        vtExfat=$(ls /lib/modules/$vtKerVer/kernel/fs/exfat/)
    fi
    [ -n "$vtExfat" ]
}

ventoy_is_exfat_part() {
    $VTOY_PATH/tool/vtoydump -s /ventoy/ventoy_os_param | $GREP -q exfat
}

ventoy_iso_scan_path() {
    if [ -f /sbin/iso-scan ]; then
        echo -n '/sbin/iso-scan'
    elif [ -f /bin/iso-scan ]; then
        echo -n '/bin/iso-scan'
    else
        echo -n ''
    fi
}

ventoy_has_iso_scan() {
    vtScanPath=$(ventoy_iso_scan_path)
    [ -n "$vtScanPath" ]
}

ventoy_rw_iso_scan() {
    vtScanPath=$(ventoy_iso_scan_path)
    if [ -n "$vtScanPath" ]; then
        if $GREP -q 'mount.* ro .*isoscan' $vtScanPath; then
            $SED -i 's/\(mount.*-o.*\) ro /\1 rw /' $vtScanPath
        fi
    fi
}

ventoy_iso_scan_check() {
    vtCheckOk=0
    if ventoy_is_exfat_part; then
        if ventoy_has_exfat_ko; then
            if ventoy_has_iso_scan; then
                vtCheckOk=1
            fi
        fi
    fi
    
    [ $vtCheckOk -eq 1 ]
}
