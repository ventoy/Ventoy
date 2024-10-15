#!/bin/bash

print_usage() {    
    echo 'Usage:  VentoyWeb.sh [ OPTION ]'   
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

uid=$(id -u)
if [ $uid -ne 0 ]; then
    print_err "Please use sudo or run the script as root."
    exit 1
fi

OLDDIR=$(pwd)

if uname -m | grep -E -q 'aarch64|arm64'; then
    TOOLDIR=aarch64
elif uname -m | grep -E -q 'x86_64|amd64'; then
    TOOLDIR=x86_64
elif uname -m | grep -E -q 'mips64'; then
    TOOLDIR=mips64el
else
    TOOLDIR=i386
fi

if [ ! -f ./tool/$TOOLDIR/V2DServer ]; then
    if [ -f ${0%VentoyWeb.sh}/tool/$TOOLDIR/V2DServer ]; then
        cd ${0%VentoyWeb.sh}
    fi
fi

PATH=./tool/$TOOLDIR:$PATH

if [ ! -f ./boot/boot.img ]; then
    if [ -d ./grub ]; then
        echo "Don't run VentoyWeb.sh here, please download the released install package, and run the script in it."
    else
        echo "Current directory is $PWD"
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

LOGFILE=log.txt
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


V2DServer "$HOST" "$PORT" &
wID=$!
sleep 1

vtVer=$(cat ventoy/version)
echo ""
echo "==============================================================="
if [ "$LANG" = "zh_CN.UTF-8" ]; then
    echo "  Ventoy Server $vtVer 已经启动 ..."
    echo "  请打开浏览器，访问 http://${HOST}:${PORT}"
else
    echo "  Ventoy Server $vtVer is running ..."
    echo "  Please open your browser and visit http://${HOST}:${PORT}"
fi
echo "==============================================================="
echo ""
echo "################## Press Ctrl + C to exit #####################"
echo ""

wait $wID

if [ -n "$OLDDIR" ]; then 
    CURDIR=$(pwd)
    if [ "$CURDIR" != "$OLDDIR" ]; then
        cd "$OLDDIR"
    fi
fi
