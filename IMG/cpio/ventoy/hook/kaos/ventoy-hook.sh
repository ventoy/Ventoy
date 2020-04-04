#!/ventoy/busybox/sh

. $VTOY_PATH/hook/ventoy-os-lib.sh

ventoy_systemd_udevd_work_around
ventoy_add_udev_rule "$VTOY_PATH/hook/kaos/udev_disk_hook.sh %k"
