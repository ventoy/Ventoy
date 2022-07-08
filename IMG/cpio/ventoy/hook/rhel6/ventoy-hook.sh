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

$BUSYBOX_PATH/mkdir -p /etc/anaconda.repos.d  /mnt/ventoy
ventoy_print_yum_repo "ventoy" "file:///mnt/ventoy" > /etc/anaconda.repos.d/ventoy.repo

ventoy_add_udev_rule "$VTOY_PATH/hook/rhel6/udev_disk_hook.sh %k"

#loop7 was used by loader
ventoy_add_kernel_udev_rule "loop6" "$VTOY_PATH/hook/rhel6/udev_disk_hook.sh %k"

if [ -f $VTOY_PATH/autoinstall ]; then
    $BUSYBOX_PATH/mv /sbin/loader /sbin/loader_bk
    $BUSYBOX_PATH/cp -a $VTOY_PATH/tool/loader /sbin/loader
    
    RawCmdLine=$($BUSYBOX_PATH/cat /proc/cmdline)
    echo -n "/sbin/loader_bk" > "/ventoy/loader_exec_file"
    echo -n "--cmdline=$RawCmdLine ks=file:$VTOY_PATH/autoinstall" > "/ventoy/loader_exec_cmdline"
    #echo 111 > "/ventoy/loader_debug"
    
    echo "/bin/sh  /ventoy/hook/rhel6/ventoy-varexp.sh" > "/ventoy/loader_hook_cmd"
fi
