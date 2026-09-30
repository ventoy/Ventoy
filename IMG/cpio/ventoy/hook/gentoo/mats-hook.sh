#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

VTPATH_OLD=$PATH; PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

for i in 0 1 2 3 4 5 6 7 8 9; do 
    vtdiskname=$(get_ventoy_disk_name)
    if [ "$vtdiskname" = "unknown" ]; then
        vtlog "wait for disk ..."
        $SLEEP 2
    else
        break
    fi
done

echo '############ Copy ISO file to memory ... ###############'

vtoydm -C -f $VTOY_PATH/ventoy_image_map -d $vtdiskname -o $VTOY_PATH/dump.iso

[ -d /ventoy/tmpmnt ] || mkdir /ventoy/tmpmnt

mount /ventoy/dump.iso /ventoy/tmpmnt
cp -a /ventoy/tmpmnt/*  $1/

umount /ventoy/tmpmnt
rm -f /ventoy/dump.iso

echo '############ Copy ISO file to memory finished OK ###############'
sleep 2

PATH=$VTPATH_OLD
