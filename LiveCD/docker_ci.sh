#!/bin/bash

VENTOY_PATH=$PWD/../

for i in vmlinuz64 corepure64.gz modules64.gz; do
    wget -q -P ISO/EFI/boot/ http://www.tinycorelinux.net/11.x/x86_64/release/distribution_files/$i
done

[ -d VTOY/ventoy/tcz ] || mkdir -p VTOY/ventoy/tcz

for i in dosfstools.tcz glib2.tcz libffi.tcz liblvm2.tcz ncursesw.tcz parted.tcz readline.tcz udev-lib.tcz; do
    wget -q -P VTOY/ventoy/tcz/ http://distro.ibiblio.org/tinycorelinux/11.x/x86_64/tcz/$i
done
