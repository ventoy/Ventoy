#!/bin/sh

set -eu

SERVICEFILE="/lib/systemd/system/vblade@.service"
WANTDIR="$1/vblade.service.wants"

CONFIG_DIR=/etc/vblade.conf.d/

if [ -d "$CONFIG_DIR" ] ; then
    mkdir -p "$WANTDIR"
    cd "$CONFIG_DIR"
    for CONFIG in *.conf ; do
        [ -f "$CONFIG" ] || continue
        INSTANCE="$(systemd-escape "${CONFIG%%.conf}")"
        LINK="$WANTDIR/vblade@$INSTANCE.service"

        sh -n "$CONFIG_DIR$CONFIG" 2>/dev/null || continue

        shelf=
        slot=
        netif=
        filename=
        options=

        . "$CONFIG_DIR$CONFIG"

        [ "$netif" ] || continue
        [ "$shelf" ] || continue
        [ "$slot" ] || continue
        [ "$filename" ] || continue

        ln -s "$SERVICEFILE" "$LINK"
    done
fi

exit 0
