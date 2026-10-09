#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

vtlog "####### $0 $* ########"

vtoy_udev_auto_hook() {
    if [ -e /etc/os-release ]; then
        if grep -q 'ID=fedora' /etc/os-release; then
            if grep -q 'VERSION_ID=[3-9][0-9]' /etc/os-release; then
                vtlog "fedora distro"
                ventoy_dracut_pivot_udev_rule
                ventoy_dracut_pivot_selinux_off
                return
            fi
        fi
    fi

    if [ -d $VT_DRACUT_HOOKS/pre-pivot ]; then
        vtlog "dracut distro"
        ventoy_dracut_pivot_udev_rule
    fi
}

vtoy_udev_auto_hook
