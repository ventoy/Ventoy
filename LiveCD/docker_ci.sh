#!/bin/bash

VENTOY_PATH=$PWD/../

for i in vmlinuz core.gz modules.gz; do
    wget -q -P ISO/EFI/boot/ http://www.tinycorelinux.net/11.x/x86/release/distribution_files/$i
done

for i in glib2.tcz libffi.tcz liblvm2.tcz ncursesw.tcz parted.tcz readline.tcz udev-lib.tcz; do
    wget -q -P VTOY/ventoy/tcz/ http://distro.ibiblio.org/tinycorelinux/11.x/x86/tcz/$i
done
