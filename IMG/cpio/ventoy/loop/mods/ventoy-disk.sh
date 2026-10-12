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

mkdir -p $VTOY_PATH/mnt/fuse $VTOY_PATH/mnt/iso $VTOY_PATH/mnt/squashfs

vtoydm -p -f $VTOY_PATH/ventoy_image_map -d $vtdiskname > $VTOY_PATH/ventoy_dm_table
vtoy_fuse_iso -f $VTOY_PATH/ventoy_dm_table -m $VTOY_PATH/mnt/fuse -w

vfree=$(losetup -f)
losetup -P $vfree $VTOY_PATH/mnt/fuse/ventoy.iso

if echo $vtdiskname | egrep -q "nvme|mmc|nbd"; then
    rm -f ${vtdiskname}p1
    cp -a ${vfree}p1 ${vtdiskname}p1
else
    rm -f ${vtdiskname}1
    cp -a ${vfree}p1 ${vtdiskname}1
fi


PATH=$VTPATH_OLD
