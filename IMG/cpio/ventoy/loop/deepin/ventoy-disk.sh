#!/bin/sh
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

cd /ventoy
xzcat tool.cpio.xz | cpio -idmu
/ventoy/tool/vtoytool/00/vtoytool_64 --install

while [ -n "Y" ]; do	
    line=$(/ventoy/tool/vtoydump -f /ventoy/ventoy_os_param)
    if [ $? -eq 0 ]; then
        vtdiskname=${line%%#*}
        break
    else    
        sleep 1
    fi
done

echo "ventoy disk is $vtdiskname" >> /ventoy/log
/ventoy/tool/vtoydm -p -f /ventoy/ventoy_image_map -d $vtdiskname > /ventoy/ventoy_dm_table
dmsetup create ventoy /ventoy/ventoy_dm_table --readonly
