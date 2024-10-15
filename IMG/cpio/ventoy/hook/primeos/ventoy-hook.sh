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

$BUSYBOX_PATH/mkdir /dev

if $GREP -q 'Detecting *PrimeOS' /init; then
    $SED '/Detecting *PrimeOS/a\ ROOT=$(cat /ventoy/rootdev)' -i /init
    $SED "/Detecting *PrimeOS/a\ $BUSYBOX_PATH/sh  $VTOY_PATH/hook/primeos/ventoy-disk.sh" -i /init
elif $GREP -q 'Detecting *PRIMEOS' /init; then
    $SED '/Detecting *PRIMEOS/a\ ROOT=$(cat /ventoy/rootdev)' -i /init
    $SED "/Detecting *PRIMEOS/a\ $BUSYBOX_PATH/sh  $VTOY_PATH/hook/primeos/ventoy-disk.sh" -i /init
else
    echo "not detecting found" >> $VTLOG
fi
