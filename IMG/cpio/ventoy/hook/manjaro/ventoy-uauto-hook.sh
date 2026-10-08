#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

vtlog "####### $0 $* ########"

vtoy_udev_auto_hook() {
    if grep -q 'break.*=.*postmount' /init; then
        vtlog "udev auto before postmount"
        sed "/break.*=.*postmount/i\\$BUSYBOX_PATH/sh $VTOY_PATH/hook/manjaro/ventoy-uauto-exec.sh"  -i /init
    fi
}

vtoy_udev_auto_hook
