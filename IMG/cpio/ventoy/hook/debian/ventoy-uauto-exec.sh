#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

vtlog "####### $0 $* ########"

subiquity_curtin_workaround() {
    local newroot=$1
    local vtDepName=''
    local vtDepDir=''
    
    mkdir -p $newroot/etc/ventoy_curtin
    ventoy_copy_file /ventoy/hook/debian/ventoy-curtin-wa.sh         $newroot/etc/ventoy_curtin/ventoy-curtin-wa.sh       0755
    ventoy_copy_file /ventoy/hook/debian/ventoy-curtin-wa.conf       $newroot/etc/ventoy_curtin/ventoy-curtin-wa.conf     0644
    ventoy_copy_file /ventoy/hook/debian/ventoy-curtin-wa.service    $newroot/etc/ventoy_curtin/ventoy-curtin-wa.service  0644

    ln -sf /etc/ventoy_curtin/ventoy-curtin-wa.service  $newroot/etc/systemd/system/ventoy-curtin-wa.service

    vtDepName=$(ls -1 $newroot/etc/systemd/system/ | grep 'snap.*subiquity-server.service' | head -n 1)
    vtDepDir=$newroot/etc/systemd/system/${vtDepName}.d
    vtlog "Depend dir is $vtDepDir"
    
    mkdir -p $vtDepDir
    ln -sf /etc/ventoy_curtin/ventoy-curtin-wa.conf $vtDepDir/10-ventoy-curtin-wa.conf
    
    sed "s#XXXX#/etc/systemd/system/${vtDepName}.d#g" -i $newroot/etc/ventoy_curtin/ventoy-curtin-wa.service
}


vroot=$(ventoy_new_root_dir)
if [ -z "$vroot" ]; then
    vtlog "new sysroot not found"
else
    ventoy_copy_udev_auto_rules
    
    if ls -l $vroot/etc/systemd/system | grep -q 'snap.*subiquity-server.service'; then
        vtlog "do snap subiquity workaround"
        subiquity_curtin_workaround $vroot
    else
        vtlog "no snap subiquity detected"
    fi
fi

if [ -d $vroot/usr/share/clonezilla ]; then
    vtlog "add cheat service for clonezilla"
    
    vtDM=$($VTOY_PATH/tool/dmsetup info VentoyPart | grep Major | sed "s/.*[^0-9]\([0-9][0-9]*\)$/\1/")
    vtRAWDISKNAME=$(head -n1 $VTOY_PATH/ventoy_raw_table | awk '{print $4}')
    
    cp -a /ventoy/hook/debian/ventoy-clonezilla-cheat.service /ventoy/udevtmp.service
    sed "s/DMXXX/dm-${vtDM}/g" -i /ventoy/udevtmp.service
    sed "s/VTISOPART/${vtRAWDISKNAME#/dev/}/g" -i /ventoy/udevtmp.service

    ventoy_copy_file /ventoy/udevtmp.service  $vroot/etc/systemd/system/ventoy-clonezilla-cheat.service  0644
    echo 'enable ventoy-clonezilla-cheat.service' > $vroot/etc/systemd/system-preset/90-ventoy-clonezilla.preset
    ln -sf /etc/systemd/system/ventoy-clonezilla-cheat.service $vroot/etc/systemd/system/sysinit.target.wants/ventoy-clonezilla-cheat.service
fi
