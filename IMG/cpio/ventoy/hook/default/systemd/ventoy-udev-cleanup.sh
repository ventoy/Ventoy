#!/bin/sh

rm -f /etc/systemd/system-preset/80-ventoy-udev.preset
rm -f /etc/systemd/system/sysinit.target.wants/ventoy-udev*.service
rm -f /etc/systemd/system/ventoy-udev*.service

if [ -f /etc/systemd/system/snap.ubuntu-desktop-bootstrap.subiquity-server.service.d/10-ventoy-curtin-wa.conf ]; then
    if [ -e /etc/systemd/system/snap.ubuntu-desktop-bootstrap.subiquity-server.service ]; then
        :
    else
        rm -f  /etc/systemd/system/ventoy-curtin-wa.service
        rm -f  /etc/systemd/system/snap.ubuntu-desktop-bootstrap.subiquity-server.service.d/10-ventoy-curtin-wa.conf
        rm -rf /etc/ventoy_curtin
    fi
fi



mkdir -p /run/udev/rules.d
mv /etc/udev/rules.d/*ventoy* /run/udev/rules.d/

rm -f $0
