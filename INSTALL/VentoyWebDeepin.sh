#!/bin/sh

print_usage() {    
    echo 'Usage:  VentoyWebDeepin.sh [ OPTION ]'   
    echo '  OPTION: (optional)'
    echo '   -H x.x.x.x  http server IP address (default is 127.0.0.1)'
    echo '   -p PORT     http server PORT (default is 24680)'
    echo '   -h          print this help'
    echo ''
}

print_err() {
    echo ""
    echo "$*"
    echo ""
}


get_user() {
    name=$(logname)
    if [ -n "$name" -a "$name" != "root" ]; then
        echo $name; return
    fi
    
    name=${HOME#/home/}
    if [ -n "$name" -a "$name" != "root" ]; then
        echo $name; return
    fi
}


uid=$(id -u)
if [ $uid -ne 0 ]; then
    exec sudo sh $0 $*
fi

OLDDIR=$(pwd)

if uname -m | egrep -q 'aarch64|arm64'; then
    TOOLDIR=aarch64
elif uname -m | egrep -q 'x86_64|amd64'; then
    TOOLDIR=x86_64
elif uname -m | egrep -q 'mips64'; then
    TOOLDIR=mips64el
else
    TOOLDIR=i386
fi

if [ ! -f ./tool/$TOOLDIR/V2DServer ]; then
    if [ -f ${0%VentoyWebDeepin.sh}/tool/$TOOLDIR/V2DServer ]; then
        cd ${0%VentoyWebDeepin.sh}
    fi
fi

PATH=./tool/$TOOLDIR:$PATH

if [ ! -f ./boot/boot.img ]; then
    if [ -d ./grub ]; then
        echo "Don't run VentoyWebDeepin.sh here, please download the released install package, and run the script in it."
    else
        echo "Please run under the correct directory!" 
    fi
    exit 1
fi

HOST="127.0.0.1"
PORT=24680

while [ -n "$1" ]; do
    if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        print_usage
        exit 0
    elif [ "$1" = "-H" ]; then
        shift
        if echo $1 | grep -q '[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*'; then
            HOST="$1"
        else
            print_err "Invalid host $1"
            exit 1
        fi        
    elif [ "$1" = "-p" ]; then
        shift
        if [ $1 -gt 0 -a $1 -le 65535 ]; then
            PORT="$1"
        else
            print_err "Invalid port $1"
            exit 1
        fi
    fi
    
    shift
done


if ps -ef | grep "V2DServer.*$HOST.*$PORT" | grep -q -v grep; then
    print_err "Another ventoy server is running now, please close it first."
    exit 1
fi

VUSER=$(get_user)

LOGFILE=log.txt
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
