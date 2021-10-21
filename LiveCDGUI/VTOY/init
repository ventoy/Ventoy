#!/bin/sh
#
# Porteus Kiosk initialization script.
# Author: T.Jokiel <http://porteus-kiosk.org>
#
# 2021 longpanda  admin@ventoy.net
#


/bin/busybox --install -s
mount -nt proc proc /proc
grep -q -w debug /proc/cmdline || { echo 0 >/proc/sys/kernel/printk 2>/dev/null; clear; }
mount -nt sysfs sysfs /sys
mount -nt devtmpfs none /dev
mkdir -p /dev/shm; chmod 1777 /dev/shm

grep -q -w debug /proc/cmdline && touch /tmp/lspci || quiet=yes

# Use memory as aufs
mount -nt tmpfs -o size=75% tmpfs /memory
mkdir -p /memory/xino /memory/changes /memory/images /memory/copy2ram

# Setup aufs
mount -nt aufs -o nowarn_perm,xino=/memory/xino/.aufs.xino,br:/memory/changes=rw aufs /union


#Draw background 
if [ -z "$quiet" ]; then
    echo "##################################################"
    echo "Starting Ventoy Live GUI <https://www.ventoy.net>"
    echo "##################################################"
else
    mkdir -p /lib /opt/000 /opt/001; lspci >/tmp/lspci
    mount -o loop /000-kernel.xzm /opt/000
    mount -o loop /001-core.xzm /opt/001
    [ `uname -m` = x86_64 ] && prefix="-x86-64"
    ln -sf /opt/000/lib/firmware /lib/firmware
    ln -sf /opt/000/lib/modules /lib/modules
    ln -sf /opt/001/lib64/libc.so.6 /lib/libc.so.6
    ln -sf /opt/001/lib64/ld-linux"$prefix".so.2 /lib/ld-linux"$prefix".so.2
    ln -s /opt/001/bin/kmod /bin/modprobe

    vga=`lspci | grep 0300: | head -n1 | cut -d: -f3-4 | sed s/:/d0000/g`; [ "$vga" ] && driver="$(grep -i $vga /lib/modules/`uname -r`/modules.alias 2>/dev/null | head -n1 | rev | cut -d" " -f1 | rev)"
    
    # Nvidia quirk:
    [ "$driver" ] || { lspci | grep 0300: | head -n1 | cut -d: -f3 | grep -q "10de" && driver=nouveau; }
    
    # VirtualBox quirk:
    [ "$driver" = vboxvideo ] || modprobe $driver 2>/dev/null
    test -e /dev/fb0 || { cp /opt/001/sbin/v86d /sbin; modprobe uvesafb mode_option=1024x768-32; }
    if test -e /dev/fb0; then
        rm -r /lib; ln -sf /opt/001/lib64 /lib; ln -sf /opt/001/usr/lib64 /usr/lib
        /opt/001/usr/bin/fbv -a -c -u -i -k -e -r /VTOY/background.png 2>/dev/null &
    fi
fi


cp -a /*.xzm /memory/copy2ram/

# Populate aufs with modules:
for x in `ls -1 /memory/copy2ram/ | grep \\.xzm$`; do
    mkdir -p /memory/images/$x
    mount -nt squashfs -o loop /memory/copy2ram/$x /memory/images/$x 2>/dev/null
    if [ $? -eq 0 ]; then
        mount -no remount,add:1:/memory/images/$x=rr aufs /union
    fi
done




#clean 
if [ -n "$quiet" ]; then
    while [ "`pidof fbv`" ]; do 
        usleep 500000
    done
    umount /opt/000 /opt/001 2>/dev/null
    rm -r /lib
fi



mkdir -p /union/opt/scripts/
echo 123 > /union/opt/scripts/extras

echo "c2::respawn:/sbin/agetty --autologin root 38400 tty2 linux" >> /union/etc/inittab
echo "c3::respawn:/sbin/agetty --autologin root 38400 tty3 linux" >> /union/etc/inittab
echo "c4::respawn:/sbin/agetty --autologin root 38400 tty4 linux" >> /union/etc/inittab


sed "s/root:[^:]*:/root::/g" -i /union/etc/shadow
rm -f /union/etc/X11/xorg.conf.d/10-xorg.conf
rm -f /union/lib64/udev/rules.d/10-kiosk-auto_mount.rules
cp -a /VTOY/autostart /union/etc/xdg/openbox/autostart
cp -a /VTOY/*.png /union/ventoy/

mkdir -p /union/usr/local/sbin
mv /VTOY/ntfs-3g.tar.gz /union/usr/local/sbin/
mv /VTOY/busybox /union/usr/local/sbin/


cp -a /bin/busybox /union/bin; ln -sf /union/lib /lib
cp -a /VTOY/wallpaper.png /union/usr/share/wallpapers/ 2>/dev/null

#to suppress error message
mkdir -p /mnt/fake/docs
echo 11 > /mnt/fake/docs/kiosk.sgn

# swith_root
exec /sbin/switch_root /union /sbin/init

