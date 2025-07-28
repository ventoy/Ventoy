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

    if $GREP -q 'endless' /proc/version; then
        echo 'endless'; return
    fi
    
    if $GREP -q 'OpenWrt' /proc/version; then
        echo 'openwrt'; return
    fi
    
    if $GREP -q 'easyos' /proc/cmdline; then
        echo 'easyos'; return
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
    
    if [ -d /twres ]; then
        if $GREP -q 'Phoenix' /init; then
            echo 'phoenixos'; return
        fi
    fi

    # Parted Magic
    if [ -d /pmagic ]; then
        echo 'pmagic'; return
    fi

    # rhel5/CentOS5 and all other distributions based on them
    if $GREP -q 'el5' /proc/version; then
        echo 'rhel5'; return

    # rhel6/CentOS6 and all other distributions based on them
    elif $GREP -q 'el6' /proc/version; then
        echo 'rhel6'; return

    # rhel7/CentOS7/rhel8/CentOS8 and all other distributions based on them
    elif $GREP -q 'el[78]' /proc/version; then
        echo 'rhel7'; return   

    # Maybe rhel9 rhel1x use the same way? Who knows!
    elif $EGREP -q 'el9|el1[0-9]' /proc/version; then
        echo 'rhel7'; return   
        
    # Fedora : do the same process with rhel7
    elif $GREP -q '\.fc[0-9][0-9]\.' /proc/version; then
        echo 'rhel7'; return
        
    # Debian :
    elif $GREP -q '[Dd]ebian' /proc/version; then
        echo 'debian'; return
        
    # Ubuntu : do the same process with debian
    elif $GREP -q '[Uu]buntu' /proc/version; then
        echo 'debian'; return
        
    # Deepin : do the same process with debian
    elif $GREP -q '[Dd]eepin' /proc/version; then
        echo 'debian'; return    

    # SUSE
    elif $GREP -q 'SUSE' /proc/version; then
        echo 'suse'; return
        
    # ArchLinux
    elif $EGREP -q 'archlinux|ARCH' /proc/version; then
        echo 'arch'; return
    
    # kiosk
    elif $EGREP -q 'kiosk' /proc/version; then
        echo 'kiosk'; return
    
    # gentoo
    elif $EGREP -q '[Gg]entoo' /proc/version; then
        if $GREP -q 'daphile' /proc/version; then
            echo 'daphile'; return
        fi
    
        echo 'gentoo'; return
        
    # TinyCore
    elif $EGREP -q 'tinycore' /proc/version; then
        echo 'tinycore'; return
    
    # manjaro
    elif $EGREP -q 'manjaro|MANJARO' /proc/version; then
        echo 'manjaro'; return
        
    # mageia
    elif $EGREP -q 'mageia' /proc/version; then
        echo 'mageia'; return
    
    # pclinux OS
    elif $GREP -i -q 'PCLinuxOS' /proc/version; then
        echo 'pclos'; return
    
    # KaOS
    elif $GREP -i -q 'kaos' /proc/version; then
        echo 'kaos'; return
    
    # Alpine
    elif $GREP -q 'Alpine' /proc/version; then
        echo 'alpine'; return

    elif $GREP -i -q 'PhoenixOS' /proc/version; then
        echo 'phoenixos'; return
    
    # NixOS
    elif $GREP -i -q 'NixOS' /proc/version; then
        echo 'nixos'; return
    
    
    fi

    if [ -e /lib/debian-installer ]; then
        echo 'debian'; return
    fi

    if [ -e /etc/os-release ]; then
        if $GREP -q 'XenServer' /etc/os-release; then
            echo 'xen'; return
        elif $GREP -q 'SUSE ' /etc/os-release; then
            echo 'suse'; return        
        elif $GREP -q 'uruk' /etc/os-release; then
            echo 'debian'; return
        elif $GREP -q 'Solus' /etc/os-release; then
            echo 'rhel7'; return
        elif $GREP -q 'openEuler' /etc/os-release; then
            echo 'openEuler'; return
        elif $GREP -q 'fuyu' /etc/os-release; then
            echo 'openEuler'; return
        elif $GREP -q 'deepin' /etc/os-release; then
            echo 'debian'; return
        elif $GREP -q 'chinauos' /etc/os-release; then
            echo 'debian'; return
        fi
    fi
    
    if $BUSYBOX_PATH/dmesg | $GREP -q -m1 "Xen:"; then
        echo 'xen'; return
    fi
    
    
    if [ -e /etc/HOSTNAME ] && $GREP -i -q 'slackware' /etc/HOSTNAME; then
        echo 'slackware'; return
    fi
    
    if [ -e /init ]; then
        if $GREP -i -q zeroshell /init; then
            echo 'zeroshell'; return
        fi
    fi
    
    if $EGREP -q 'ALT ' /proc/version; then
        echo 'alt'; return
    fi
    
    if $EGREP -q 'porteus' /proc/version; then
        echo 'debian'; return
    fi
    
    if $GREP -q 'Clear Linux ' /proc/version; then
        echo 'clear'; return
    fi
    
    if $GREP -q 'artix' /proc/version; then
        echo 'arch'; return
    fi
    
    if $GREP -q 'berry ' /proc/version; then
        echo 'berry'; return
    fi
    
    if $GREP -q 'Gobo ' /proc/version; then
        echo 'gobo'; return
    fi
    
    if $GREP -q 'NuTyX' /proc/version; then
        echo 'nutyx'; return
    fi
    
    if [ -d /gnu ]; then
        vtLineNum=$($FIND /gnu/ -name guix | $BUSYBOX_PATH/wc -l)
        if [ $vtLineNum -gt 0 ]; then
            echo 'guix'; return
        fi
    fi
    
    if $GREP -q 'android.x86' /proc/version; then
        echo 'android'; return
    fi 
    
    if $GREP -q 'adelielinux' /proc/version; then
        echo 'adelie'; return
    fi
    
    if $GREP -q 'CDlinux' /proc/cmdline; then
        echo 'cdlinux'; return
    fi
    
    if $GREP -q 'parabola' /proc/version; then
        echo 'parabola'; return
    fi
    
    if $GREP -q 'cucumber' /proc/version; then
        echo 'cucumber'; return
    fi
    
    if $GREP -q 'fatdog' /proc/version; then
        echo 'fatdog'; return
    fi
    
    if $GREP -q 'KWORT' /proc/version; then
        echo 'kwort'; return
    fi
    
    if $GREP -q 'iwamoto' /proc/version; then
        echo 'vine'; return
    fi
    
    if $GREP -q 'hyperbola' /proc/cmdline; then
        echo 'hyperbola'; return
    fi
    
    if $GREP -q 'CRUX' /proc/version; then
        echo 'crux'; return
    fi
    
    if [ -f /init ]; then
        if $GREP -q 'AryaLinux' /init; then
            echo 'aryalinux'; return
        elif $GREP -q 'Dragora' /init; then
            echo 'dragora'; return
            
        fi
    fi
    
    if $GREP -q 'slackware' /proc/version; then
        echo 'slackware'; return
    fi
    
    if $BUSYBOX_PATH/hostname | $GREP -q 'smoothwall'; then
        echo 'smoothwall'; return
    fi 
    
    if $GREP -q 'photon' /proc/version; then
        echo 'photon'; return
    fi
    
    if $GREP -q 'ploplinux' /proc/version; then
        echo 'ploplinux'; return
    fi
    
    if $GREP -q 'lunar' /proc/version; then
        echo 'lunar'; return
    fi
    
    if $GREP -q 'SMGL-' /proc/version; then
        echo 'smgl'; return
    fi
    
    if $GREP -q 'rancher' /proc/version; then
        echo 'rancher'; return
    fi
    
    
    if [ -e /init ]; then
        if $GREP -q -m1 'T2 SDE' /init; then
            echo 't2'; return
        fi
    fi
    
    if $GREP -q 'wifislax' /proc/version; then
        echo 'wifislax'; return
    fi
    
    if $GREP -q 'pisilinux' /proc/version; then
        echo 'pisilinux'; return
    fi
    
    if $GREP -q 'blackPanther' /proc/version; then
        echo 'blackPanther'; return
    fi
    
    if $GREP -q 'primeos' /proc/version; then
        echo 'primeos'; return
    fi
    
    if $GREP -q 'austrumi' /proc/version; then
        echo 'austrumi'; return
    fi
    
    if [ -f /DISTRO_SPECS ]; then
        if $GREP -q '[Pp]uppy' /DISTRO_SPECS; then
            echo 'debian'; return
        elif $GREP -q 'veket' /DISTRO_SPECS; then
            echo 'debian'; return
        fi
    fi
    
    if [ -f /etc/openEuler-release ]; then
        echo "openEuler"; return
    fi
    
    echo "default"
}

VTOS=$(ventoy_get_os_type)
echo "OS=###${VTOS}###" >>$VTLOG
if [ -e "$VTOY_PATH/loop/$VTOS/ventoy-hook.sh" ]; then
    $BUSYBOX_PATH/sh "$VTOY_PATH/loop/$VTOS/ventoy-hook.sh"
elif [ -e "$VTOY_PATH/hook/$VTOS/ventoy-hook.sh" ]; then
    $BUSYBOX_PATH/sh "$VTOY_PATH/hook/$VTOS/ventoy-hook.sh"
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

if [ -f $VTOY_PATH/ventoy_persistent_map ]; then
    export PERSISTENT='YES'
    export PERSISTENCE='true'
fi

cd /

unset VTLOG FIND GREP EGREP CAT AWK SED SLEEP HEAD vtcmdline

for vtinit in $user_rdinit /init /sbin/init /linuxrc; do
    if [ -d /ventoy_rdroot ]; then
        if [ -e "/ventoy_rdroot$vtinit" ]; then
            # switch_root will check /init file, this is a cheat code
            echo 'switch_root' > /init
            exec $BUSYBOX_PATH/switch_root /ventoy_rdroot "$vtinit"
        fi
    else
        if [ -e "$vtinit" ];then
            if [ -f "$VTOY_PATH/hook/$VTOS/ventoy-before-init.sh" ]; then
                $BUSYBOX_PATH/sh "$VTOY_PATH/hook/$VTOS/ventoy-before-init.sh"
            fi
            exec "$vtinit"
        fi
    fi
done

# Should never reach here
echo -e "\n\n\033[31m ############ INIT NOT FOUND ############### \033[0m \n"
exec $BUSYBOX_PATH/sh
