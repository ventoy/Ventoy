#!/bin/bash

exec >> /var/log/ventoy-curtin-wa.log 2>&1
set -x

if [ -e /tmp/ventoy_storage_config.py ]; then
    echo "no need to patch again"
elif [ -d /snap/ubuntu-desktop-bootstrap/current/lib ]; then
    PyVer=$(ls -1 /snap/ubuntu-desktop-bootstrap/current/lib | grep -m1 python)
    echo "Python Path: $PyVer"

    OldFile=/snap/ubuntu-desktop-bootstrap/current/lib/$PyVer/site-packages/curtin/storage_config.py
    NewFile=/tmp/ventoy_storage_config.py

    if [ -e $OldFile ]; then
        echo "Patch curtin storage config script"
        cp -a $OldFile $NewFile

        #skip Ventoy dm device
        sed 's#\(if *\(.*\)\[.MAJOR.\] *in *\["11", *"2"\]\)#\1 or re.match(r"[vV]entoy.*|vtoy.*", \2.get("DM_NAME") or "")#g' -i $NewFile
        mount --bind $NewFile $OldFile
    else
        echo "curin file not exist"
    fi
else
    echo "curin snap dir not exit"
fi
