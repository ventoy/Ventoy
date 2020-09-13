#!/bin/sh

cd /
tar -xf ventoy.tar.gz

cd /ventoy
mkdir mnt
for i in $(ls tcz/*.tcz); do
    mount $i mnt
    cp -a mnt/* /
    umount mnt
done

ldconfig /usr/local/lib /usr/lib /lib

#workaround for swapon
rm -f /sbin/swapon
echo '#!/bin/sh' > /sbin/swapon
chmod +x /sbin/swapon

sh /ventoy/ventoy.sh

