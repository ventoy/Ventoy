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

vlog() {
    echo "$@" >> /ventoy/autoinstall.log
}

if grep -q '\$\$VT_' /ventoy/autoinstall; then
    vlog "======== auto install variables expansion ======="
else
    vlog "======== auto install variables expansion no need ======="
    exit 0
fi

if [ -f /ventoy/ventoy_os_param ]; then
    VTOYDISK=$(vtoydump -f /ventoy/ventoy_os_param | awk -F'#' '{print $1}')
    vlog VTOYDISK=$VTOYDISK
    
    if [ -b "$VTOYDISK" ]; then
        vlog "$VTOYDISK exist OK"
    else
        vlog "$VTOYDISK does NOT exist"
        exit 0
    fi
    
    if [ -n "$1" -a -f "$1" ]; then
        vtoyexpand "$1" "$VTOYDISK"
    else
        vlog "File $1 not exist"
    fi    
else
    vlog "os param file not exist"
    exit 0
fi

