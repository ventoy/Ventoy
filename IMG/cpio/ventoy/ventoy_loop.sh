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

####################################################################
#                                                                  #
# Step 1 : Parse kernel parameter                                  #
#                                                                  #
####################################################################
if ! [ -e /proc ]; then
    $BUSYBOX_PATH/mkdir /proc
    rmproc='Y'
fi
$BUSYBOX_PATH/mount -t proc proc /proc

# vtinit=xxx to replace rdinit=xxx
vtcmdline=$($CAT /proc/cmdline)
for i in $vtcmdline; do
    if echo $i | $GREP -q vtinit; then
        user_rdinit=${i#vtinit=}
        echo "user set user_rdinit=${user_rdinit}" >>$VTLOG
    fi
done

####################################################################
#                                                                  #
# Step 2 : Process ko                                              #
#                                                                  #
####################################################################
$BUSYBOX_PATH/mkdir -p /ventoy/modules
$BUSYBOX_PATH/ls -1a / | $EGREP '\.ko$|\.ko.[gx]z$' | while read vtline; do
    if [ "${vtline:0:1}" = "." ]; then
        $BUSYBOX_PATH/mv /${vtline} /ventoy/modules/${vtline:1}
    else
        $BUSYBOX_PATH/mv /${vtline} /ventoy/modules/
    fi
done

if [ -e /vtloopex.tar.xz ]; then
    echo "extract vtloopex ..." >> $VTLOG
    $BUSYBOX_PATH/tar -xJf /vtloopex.tar.xz -C $VTOY_PATH/
    $BUSYBOX_PATH/rm -f /vtloopex.tar.xz
fi


####################################################################
#                                                                  #
# Step 3 : Do OS specific hook                                     #
#                                                                  #
####################################################################
ventoy_get_os_type() {
    echo "kernel version" >> $VTLOG
    $CAT /proc/version >> $VTLOG

    # deepin-live
    if $GREP -q 'deepin' /proc/version; then
        echo 'deepin'; return
    fi
    
    if $GREP -q 'endless' /proc/version; then
        echo 'endless'; return
    fi
    
    if $GREP -q 'OpenWrt' /proc/version; then
        echo 'openwrt'; return
    fi
    
    if [ -e /BOOT_SPECS ]; then
        if $GREP -q 'easyos' /BOOT_SPECS; then
            echo 'easyos'; return
        fi
    fi
    
    if [ -e /etc/os-release ]; then
        if $GREP -q 'volumio' /etc/os-release; then
            echo 'volumio'; return
        fi
    fi
    
    if $GREP -q 'ventoyos=' /proc/cmdline; then
        $SED "s/.*ventoyos=\([a-zA-Z0-9_-]*\).*/\1/" /proc/cmdline; return 
    fi    
    
    echo "default"
}

VTOS=$(ventoy_get_os_type)
echo "OS=###${VTOS}###" >>$VTLOG
if [ -e "$VTOY_PATH/loop/$VTOS/ventoy-hook.sh" ]; then
    $BUSYBOX_PATH/sh "$VTOY_PATH/loop/$VTOS/ventoy-hook.sh"
fi


####################################################################
#                                                                  #
# Step 4 : Check for debug break                                   #
#                                                                  #
####################################################################
if [ "$VTOY_BREAK_LEVEL" = "03" ] || [ "$VTOY_BREAK_LEVEL" = "13" ]; then
    $SLEEP 5
    echo -e "\n\n\033[32m ################################################# \033[0m"
    echo -e "\033[32m ################ VENTOY DEBUG ################### \033[0m"
    echo -e "\033[32m ################################################# \033[0m \n"
    if [ "$VTOY_BREAK_LEVEL" = "13" ]; then 
        $CAT $VTOY_PATH/log
    fi
    exec $BUSYBOX_PATH/sh
fi


####################################################################
#                                                                  #
# Step 5 : Hand over to real init                                  #
#                                                                  #
####################################################################
$BUSYBOX_PATH/umount /proc
if [ "$rmproc" = "Y" ]; then
    $BUSYBOX_PATH/rm -rf /proc
fi

cd /

unset VTLOG FIND GREP EGREP CAT AWK SED SLEEP HEAD

for vtinit in $user_rdinit /init /sbin/init  /linuxrc; do
    if [ -d /ventoy_rdroot ]; then
        if [ -e "/ventoy_rdroot$vtinit" ]; then
            # switch_root will check /init file, this is a cheat code
            echo 'switch_root' > /init
            exec $BUSYBOX_PATH/switch_root /ventoy_rdroot "$vtinit"
        fi
    else
        if [ -e "$vtinit" ];then
            exec "$vtinit"
        fi
    fi
done

# Should never reach here
echo -e "\n\n\033[31m ############ INIT NOT FOUND ############### \033[0m \n"
exec $BUSYBOX_PATH/sh
