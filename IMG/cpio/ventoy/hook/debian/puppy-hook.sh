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

$SED '1 apmedia=usbhd'  -i /init

if $GREP -q 'HAVE_PARTS=' /init; then
    $SED "/^ *HAVE_PARTS=/a\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/puppy-disk.sh"  -i /init
    $SED "/^ *HAVE_PARTS=/a\ HAVE_PARTS='ventoy|iso9660'"  -i /init
elif $GREP -q 'LESSPARTS=' /init; then
    $SED "/^ *LESSPARTS=/a\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/puppy-disk.sh"  -i /init
    $SED "/^ *LESSPARTS=/a\ LESSPARTS='ventoy|iso9660'"  -i /init
fi


if [ -f /DISTRO_SPECS ]; then
    if ! [ -d /dev ]; then
        $BUSYBOX_PATH/mkdir /dev
    fi
fi
