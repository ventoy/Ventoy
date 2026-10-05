#!/bin/sh

for vroot in sysroot newroot; do
    if [ -d $vroot/etc ]; then
        if [ -d $vroot/etc/udev/rules.d ]; then
            :
        else
            mkdir -p $vroot/etc/udev/rules.d
        fi
        
        cp -a /ventoy/hook/default/90-ventoy-auto.rules  $vroot/etc/udev/rules.d/
        chmod 0644 $vroot/etc/udev/rules.d/90-ventoy-auto.rules
        break
    fi
done
