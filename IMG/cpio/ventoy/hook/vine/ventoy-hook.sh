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

$BUSYBOX_PATH/cp -a /sbin/loader /sbin/loader_bk
$VTOY_PATH/tool/vine_patch_loader /sbin/loader 253 -v >> $VTLOG

$BUSYBOX_PATH/mknod -m 0660 /dev/null c 1 3
$VTOY_PATH/hook/vine/dev-listen.sh  &  
