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

PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

LineBegin=$(grep -n "ebegin.*Mounting boot media" /init | awk -F: '{print $1}')

grep -n "^eend" /init > /t.list
while read line; do
    LineEnd=$(echo $line | awk -F: '{print $1}')
    if [ $LineEnd -gt $LineBegin ]; then
        sed "${LineEnd}i\done" -i /init
        sed "${LineBegin}r /ventoy/hook/alpine/insert.sh" -i /init        
        break
    fi
done < /t.list
rm -f /t.list
