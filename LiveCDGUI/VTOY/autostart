#!/bin/sh

hsetroot -fill /usr/share/wallpapers/wallpaper.png

INIFILE=/ventoy/Ventoy2Disk.ini

echo "[Ventoy]"                                 >> $INIFILE
echo "PartStyle=0"                              >> $INIFILE
echo "ShowAllDevice=0"                          >> $INIFILE


VTOOLDIR=/ventoy/tool/x86_64

ls -1 $VTOOLDIR/ | grep '\.xz$' | while read line; do
    $VTOOLDIR/xzcat $VTOOLDIR/$line > $VTOOLDIR/${line%.xz}
    rm -f $VTOOLDIR/$line
    chmod +x $VTOOLDIR/${line%.xz}
done

cp -a $VTOOLDIR/mount.exfat-fuse /bin/mount.exfat
cp -a $VTOOLDIR/mkexfatfs /bin/mkfs.exfat

/usr/local/sbin/busybox --install /usr/local/sbin/
tar xf /usr/local/sbin/ntfs-3g.tar.gz -C /

/ventoy/tool/x86_64/Ventoy2Disk.gtk3 --kiosk

reboot -f
