#!/bin/sh

cat /ventoy/modlist | while read line; do
    if [ -e /ventoy/drivers/${line}.ko ]; then
        insmod /ventoy/drivers/${line}.ko
    fi
done

sleep 5

echo "sh /ventoy/profile.sh" >> /root/.profile
exec /init
