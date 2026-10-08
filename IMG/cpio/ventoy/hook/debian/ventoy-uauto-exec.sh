#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

PATH=$BUSYBOX_PATH:$VTOY_PATH/tool:$PATH

vtlog "####### $0 $* ########"

subiquity_curtin_workaround() {
    mkdir -p /root/etc/ventoy
    ventoy_copy_file /ventoy/hook/debian/ventoy-curtin-wa.sh        /root/etc/ventoy/ventoy-curtin-wa.sh       0755
    ventoy_copy_file /ventoy/hook/debian/ventoy-curtin-wa.conf      /root/etc/ventoy/ventoy-curtin-wa.conf     0644
    ventoy_copy_file /ventoy/hook/debian/ventoy-curtin-wa.service   /root/etc/ventoy/ventoy-curtin-wa.service  0644

    ln -sf /etc/ventoy/ventoy-curtin-wa.service /root/etc/systemd/system/ventoy-curtin-wa.service

    vtDepDir=/root/etc/systemd/system/snap.ubuntu-desktop-bootstrap.subiquity-server.service.d
    mkdir -p $vtDepDir
    ln -sf /etc/ventoy/ventoy-curtin-wa.conf $vtDepDir/10-ventoy-curtin-wa.conf
}


vroot=$(ventoy_new_root_dir)
if [ -z "$vroot" ]; then
    vtlog "new sysroot not found"
else
    ventoy_copy_udev_auto_rules
    if ls $vroot/etc/udev/rules.d | grep -q 'snap.*desktop'; then
        subiquity_curtin_workaround
    else
        vtlog "no snap desktop detected"
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
