#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

vtlog "####### $0 $* ########"

vtoy_udev_auto_hook() {
    if grep -q 'break.*=.*postmount' /init; then
        vtlog "udev auto before postmount"
        sed "/break.*=.*postmount/i\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/hyperbola/ventoy-uauto-exec.sh"  -i /init
    elif [ -d $VT_DRACUT_HOOKS/pre-pivot ]; then
        vtlog "dracut distro"
        ventoy_dracut_pivot_udev_rule
    fi
}

vtoy_udev_auto_hook
