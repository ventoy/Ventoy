#!/bin/bash

exec >> /var/log/ventoy-curtin-wa.log 2>&1
set -x

if [ -e /tmp/ventoy_storage_config.py ]; then
    echo "no need to patch again"
else
    NewFile=/tmp/ventoy_storage_config.py
    OldFile=$(find /snap -type f -path '*curtin/storage_config.py' -print -quit 2>/dev/null)
    if [ -n "$OldFile" ]; then
        echo "Patch curtin storage config script"
        cp -a $OldFile $NewFile

        #skip Ventoy dm device
        sed 's#\(if *\(.*\)\[.MAJOR.\] *in *\["11", *"2"\]\)#\1 or re.match(r"[vV]entoy.*|vtoy.*", \2.get("DM_NAME") or "")#g' -i $NewFile
        mount --bind $NewFile $OldFile
    else
        echo "curin file not exist"
    fi
fi
