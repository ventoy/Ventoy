#!/bin/sh

rm -f /etc/systemd/system-preset/80-ventoy-udev.preset
rm -f /etc/systemd/system/sysinit.target.wants/ventoy-udev*.service
rm -f /etc/systemd/system/ventoy-udev*.service


mkdir -p /run/udev/rules.d
mv /etc/udev/rules.d/*ventoy* /run/udev/rules.d/

rm -f $0
