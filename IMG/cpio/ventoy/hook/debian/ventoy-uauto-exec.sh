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


if [ -d /root/etc/systemd/system -a -d /root/etc/udev/rules.d ]; then
    vtlog "systemd udev check OK"

    ventoy_copy_udev_auto_rules

    if ls /root/etc/udev/rules.d | grep -q 'snap.*desktop'; then
        subiquity_curtin_workaround
    else
        vtlog "no snap desktop detected"
    fi
else
    vtlog "systemd or udev not exist"
fi

