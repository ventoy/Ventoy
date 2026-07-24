#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

vtlog "######### $0 $* ############"

if is_ventoy_hook_finished; then
    exit 0
fi

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    exit 0
fi

vtlog "vtdiskname=$vtdiskname"

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2" "noreplace"

# OK finish
set_ventoy_hook_finish
