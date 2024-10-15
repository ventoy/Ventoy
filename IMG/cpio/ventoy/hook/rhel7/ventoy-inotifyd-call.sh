#!/ventoy/busybox/sh
#************************************************************************************
# Copyright (c) 2023, longpanda <admin@ventoy.net>
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
vtlog "... $vtdisk already exist ..."
$BUSYBOX_PATH/sh $vtHook n /dev "${vtdisk#/dev/}2"    
