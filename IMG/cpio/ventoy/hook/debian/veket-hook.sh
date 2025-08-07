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
$SED "/^ *HAVE_PARTS=/a\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/veket-disk.sh"  -i /init
$SED "/^ *HAVE_PARTS=/a\ HAVE_PARTS='ventoy|iso9660'"  -i /init

if [ -d /dev ]; then
    [ -e /dev/null ] || $BUSYBOX_PATH/mknod /dev/null c 1 3
else
    $BUSYBOX_PATH/mkdir /dev
    $BUSYBOX_PATH/mknod /dev/null c 1 3
fi
