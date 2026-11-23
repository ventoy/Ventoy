#!/bin/sh

set +e

rm -f /etc/boot.d/S01-ventoy-udev-trigger*
rm -f /etc/runlevels/sysinit/ventoy-udev-trigger*
rm -f /etc/runlevels/default/ventoy-udev-trigger*
rm -f /etc/dinit.d/boot.d/ventoy-udev-trigger*
rm -f /etc/dinit.d/default/ventoy-udev-trigger*
rm -f /etc/runit/runsvdir/sysinit/ventoy-udev-trigger*
rm -f /etc/runit/runsvdir/default/ventoy-udev-trigger*
rm -f /etc/s6/adminsv/sysinit/contents.d/ventoy-udev-trigger*

rm -f /etc/init.d/ventoy-udev-trigger*
rm -f /etc/dinit.d/ventoy-udev-trigger*
rm -rf /etc/runit/sv/ventoy-udev-trigger*
rm -rf /etc/s6/sv/ventoy-udev-trigger*


if ls -l /etc/udev/rules.d/ | grep -q ventoy; then
    mkdir -p /run/udev/rules.d
    mv /etc/udev/rules.d/*ventoy* /run/udev/rules.d/
fi


if [ -f /etc/rc.local ]; then
    if grep -q ventoy /etc/rc.local; then
        sed -i '/ventoy/d' /etc/rc.local
    fi
fi


if [ -f /etc/s6/rc.local ]; then
    if grep -q ventoy /etc/s6/rc.local; then
        sed -i '/ventoy/d' /etc/s6/rc.local
    fi
fi


setsid sh -c '
    count=0
    while [ $count -lt 60 ]; do
        if pidof udisksd >/dev/null 2>&1; then
            udevadm trigger --action=change --subsystem-match=block --sysname-match=yyy
            udevadm trigger --action=change --subsystem-match=block --sysname-match=DMXXX
            sleep 1
            break
        fi
        count=$((count + 1))
        sleep 2
    done
' &


rm -f $0


