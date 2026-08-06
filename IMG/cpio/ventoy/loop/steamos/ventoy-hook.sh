#!/ventoy/busybox/sh
#************************************************************************************
# Copyright (c) 2026, longpanda <admin@ventoy.net>
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

# inject the disk hook before the run_hook phase of the stock mkinitcpio /init
if $GREP -q "run_hookfunctions 'run_hook' 'hook'" /init; then
    echo "insert ventoy-disk.sh before run_hook" >> $VTLOG
    $SED "/run_hookfunctions 'run_hook' 'hook'/i\\$BUSYBOX_PATH/sh $VTOY_PATH/loop/steamos/ventoy-disk.sh" -i /init
else
    echo "run_hook anchor not found in /init" >> $VTLOG
fi
