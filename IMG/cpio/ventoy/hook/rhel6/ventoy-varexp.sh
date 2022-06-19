#!/bin/sh
#************************************************************************************
# Copyright (c) 2022, longpanda <admin@ventoy.net>
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

PATH=$PATH:/ventoy/busybox:/ventoy/tool

if grep -q '\$\$VT_' /ventoy/autoinstall; then
    :
else
    exit 0
fi

if [ -f /sbin/hald ]; then
    mv /sbin/hald /sbin/hald_bk
    cp -a /ventoy/tool/hald /sbin/hald

    rm -f "/ventoy/loader_exec_cmdline"
    echo "/bin/sh  /ventoy/hook/default/auto_install_varexp.sh /ventoy/autoinstall" > "/ventoy/loader_hook_cmd"
    echo -n "/sbin/hald_bk" > "/ventoy/loader_exec_file"
fi

exit 0
