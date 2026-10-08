#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

vtlog "####### $0 $* ########"

vtoy_udev_auto_hook() {
    if [ -e /etc/os-release ]; then
        if grep -q 'ID=ubuntu' /etc/os-release; then
            if egrep -q 'VERSION_ID=\"2[2-9]\.|VERSION_ID=\"3[0-9]\.' /etc/os-release; then
                vtlog "ubuntu distro"
                sed "/^mountroot$/a\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/ventoy-uauto-exec.sh"  -i /init
                return
            fi
        fi
    fi
    
    
    if [ -e /init ]; then
        if $GREP -q '^mountroot$' /init; then
            vtlog "mountroot distro"
            sed "/^mountroot$/a\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/ventoy-uauto-exec.sh"  -i /init
            return            
        fi
        
        if $GREP -q '^ *exec.*switch_root' /init; then
            vtlog "switch_root distro"
            sed "/^ *exec.*switch_root/i\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/debian/ventoy-uauto-exec.sh"  -i /init
            return
        fi        
    fi
}

vtoy_udev_auto_hook
