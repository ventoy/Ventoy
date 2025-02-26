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

if [ -e /init ] && $GREP -q '^mountroot$' /init; then
    echo "Here before mountroot ..." >> $VTLOG    
    $SED  "/^mountroot$/i\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/truenas-disk.sh"  -i /init
    $SED  "/^mountroot$/i\\export LIVEMEDIA=/dev/mapper/ventoy"  -i /init
    $SED  "/^mountroot$/i\\export LIVE_MEDIA=/dev/mapper/ventoy"  -i /init    
    $SED  "/^mountroot$/i\\export FROMISO=$VTOY_PATH/mnt/fuse/ventoy.iso"  -i /init    
fi
