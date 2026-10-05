#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

vtlog "####### $0 $* ########"

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

wait_for_usb_disk_ready

vtdiskname=$(get_ventoy_disk_name)
if [ "$vtdiskname" = "unknown" ]; then
    vtlog "ventoy disk not found"
    PATH=$VTPATH_OLD
    exit 0
fi

ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2"

mkdir -p /ventoy/mnt/iso /ventoy/mnt/sfs

vtDM=$(dmsetup info ventoy | grep minor | sed 's/.*\([0-9][0-9]*\)/\1/')

vtlog "mount -o ro -t iso9660 /dev/dm-${vtDM} /ventoy/mnt/iso"
mount -o ro -t iso9660 /dev/dm-${vtDM} /ventoy/mnt/iso 

mount /ventoy/mnt/iso/casper/minimal.standard.live.squashfs /ventoy/mnt/sfs

mount --bind /ventoy/mnt/sfs/usr/lib/modules /lib/modules

ventoy_load_iso_part_fs_ko

umount /lib/modules
umount /ventoy/mnt/sfs
umount /ventoy/mnt/iso


ventoy_remove_all_dm

if ventoy_mount_iso "${vtdiskname}"; then
    vtlog "direcly mount iso partition SUCCESS"
else
    vtlog "direcly mount iso partition FAILED, keep use DM"
    ventoy_udev_disk_common_hook "${vtdiskname#/dev/}2"
fi


if [ -f /ventoy/autoinstall ]; then
    sh /ventoy/hook/default/auto_install_varexp.sh  /ventoy/autoinstall
fi

set_ventoy_hook_finish
