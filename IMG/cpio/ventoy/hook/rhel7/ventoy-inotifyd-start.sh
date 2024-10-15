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

vtHook=$($CAT $VTOY_PATH/inotifyd-hook-script.txt)

vtdisk=$(get_ventoy_disk_name)
if [ "$vtdisk" = "unknown" ]; then
    vtlog "... start inotifyd listen $vtHook ..."
    $BUSYBOX_PATH/nohup $VTOY_PATH/tool/inotifyd $vtHook  /dev:n  2>&-  & 
else
    vtlog "... $vtdisk already exist ..."
    
    #don't call it too early issue 2225
    #$BUSYBOX_PATH/sh $vtHook n /dev "${vtdisk#/dev/}2"
    cp -a  $VTOY_PATH/hook/rhel7/ventoy-inotifyd-call.sh /lib/dracut/hooks/initqueue/settled/90-ventoy-inotifyd-call.sh   
fi
