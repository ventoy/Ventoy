#!/bin/sh

LOGFILE=log.txt
VUSER=$(get_user)

if which browser >/dev/null 2>&1; then
    :
else
    if [ "$LANG" = "zh_CN.UTF-8" ]; then
        echo "  Built-in browser not found in the system, please use VentoyWeb.sh ..."
    else
        echo "  未找到系统内置的 browser （卸载了？）请使用 VentoyWeb.sh ..."
    fi
    exit 1
fi


if [ -e $LOGFILE ]; then
    chown $VUSER $LOGFILE
else
    su $VUSER -c "touch $LOGFILE"
fi

#delete the log.txt if it's more than 8MB
if [ -f $LOGFILE ]; then
    logsize=$(stat -c '%s' $LOGFILE)
    if [ $logsize -gt 8388608 ]; then
        rm -f $LOGFILE
    fi
fi


if [ -f ./tool/$TOOLDIR/V2DServer.xz ]; then
    xz -d ./tool/$TOOLDIR/V2DServer.xz
    chmod +x ./tool/$TOOLDIR/V2DServer
fi

rm -rf ./*_VTMPDIR
vtWebTmpDir=$(mktemp -d -p ./ --suffix=_VTMPDIR)
chown $VUSER $vtWebTmpDir


V2DServer "$HOST" "$PORT" &
V2DPid=$!
sleep 1

su $VUSER -c "browser --window-size=550,400 --app=\"http://${HOST}:${PORT}/index.html?chrome-app\"  --user-data-dir=$vtWebTmpDir >> $LOGFILE 2>&1" &
WebPid=$!


vtoy_trap_exit() {

    [ -d /proc/$V2DPid ] && kill -2 $V2DPid
    [ -d /proc/$WebPid ] && kill -9 $WebPid

    while [ -n "1" ]; do
        curPid=$(ps -ef | grep -m1 "$vtWebTmpDir" | egrep -v '\sgrep\s' | awk '{print $2}')
        if [ -z "$curPid" ]; then
            break
        fi
        
        if [ -d /proc/$curPid ]; then
            kill -9 $curPid
        fi
    done

    [ -d $vtWebTmpDir ] && rm -rf $vtWebTmpDir

    if [ -n "$OLDDIR" ]; then 
        CURDIR=$(pwd)
        if [ "$CURDIR" != "$OLDDIR" ]; then
            cd "$OLDDIR"
        fi
    fi

    exit 1
}

trap vtoy_trap_exit HUP INT QUIT TSTP
sleep 1


vtVer=$(cat ventoy/version)
echo ""
echo "=================================================="
if [ "$LANG" = "zh_CN.UTF-8" ]; then
    echo "  Ventoy Server $vtVer 已经启动 ..."
else
    echo "  Ventoy Server $vtVer is running ..."
fi
echo "=================================================="
echo ""
echo "########### Press Ctrl + C to exit ###############"
echo ""

wait $WebPid

[ -d /proc/$V2DPid ] && kill -2 $V2DPid

[ -d $vtWebTmpDir ] && rm -rf $vtWebTmpDir

if [ -n "$OLDDIR" ]; then 
    CURDIR=$(pwd)
    if [ "$CURDIR" != "$OLDDIR" ]; then
        cd "$OLDDIR"
    fi
fi
