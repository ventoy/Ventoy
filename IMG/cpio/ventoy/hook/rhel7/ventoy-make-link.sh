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

if ! [ -e /dev/mapper/ventoy ]; then
    vtlog "link to /dev/mapper/ventoy"
    ln -s /dev/dm-0 /dev/mapper/ventoy
fi

VTLABEL=$($BUSYBOX_PATH/blkid /dev/dm-0 | $SED 's/.*LABEL="\([^"]*\)".*/\1/')
vtlog "VTLABEL=$VTLABEL"

if [ -n "$VTLABEL" ]; then
    if ! [ -e "/dev/disk/by-label/$VTLABEL" ]; then
        vtlog "link to /dev/disk/by-label/$VTLABEL"
        ln -s /dev/dm-0 "/dev/disk/by-label/$VTLABEL"
    fi
fi
