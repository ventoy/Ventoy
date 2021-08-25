#!/ventoy/busybox/sh

. $VTOY_PATH/hook/ventoy-os-lib.sh

if $GREP '^mediadetected=' /init; then
    $SED "/^mediadetected=/i $BUSYBOX_PATH/sh $VTOY_PATH/hook/slackware/disk_hook.sh" -i /init
else
    ventoy_systemd_udevd_work_around
    ventoy_add_udev_rule "$VTOY_PATH/hook/slackware/udev_disk_hook.sh %k noreplace"
fi


