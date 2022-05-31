for i in 1 2; do 
if [ $i -eq 2 ]; then
    /ventoy/busybox/sh /ventoy/hook/alpine/udev_disk_hook.sh
fi
