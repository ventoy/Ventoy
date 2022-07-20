#!/ventoy/busybox/sh

. $VTOY_PATH/hook/ventoy-os-lib.sh

if $GREP -q '^"$mount_handler"' /init; then
    echo 'use mount_handler1 ...' >> $VTLOG
    $SED "/^\"\$mount_handler\"/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/kaos/ventoy-disk.sh" -i /init    
elif $GREP -q '^$mount_handler' /init; then
    echo 'use mount_handler2 ...' >> $VTLOG
    $SED "/^\$mount_handler/i\ $BUSYBOX_PATH/sh $VTOY_PATH/hook/kaos/ventoy-disk.sh" -i /init    
fi

if [ -f $VTOY_PATH/ventoy_persistent_map ]; then
    $SED "1 aexport cow_label=vtoycow" -i /init 
fi
