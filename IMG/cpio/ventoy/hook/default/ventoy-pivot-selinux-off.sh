#!/ventoy/busybox/sh

. /ventoy/hook/ventoy-hook-lib.sh

vtlog "####### $0 $* ########"

for vroot in sysroot newroot root; do
    if [ -e $vroot/etc/selinux/config ]; then
        $SED "s/SELINUX=enforcing/SELINUX=permissive/g" -i $vroot/etc/selinux/config
        break
    fi
done

$BUSYBOX_PATH/true
