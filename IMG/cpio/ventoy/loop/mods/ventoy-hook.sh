#!/ventoy/busybox/sh

. $VTOY_PATH/hook/ventoy-os-lib.sh

$SED "/FOUNDDEV=\"\"/a\ $BUSYBOX_PATH/sh $VTOY_PATH/loop/mods/ventoy-disk.sh" -i /init
