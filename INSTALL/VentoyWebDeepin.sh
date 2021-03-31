#!/bin/sh

if echo "$*" | grep -q '[-]v'; then
    set -x
fi

print_usage() {    
    echo 'Usage:  VentoyWebDeepin.sh [ OPTION ]'   
    echo '  OPTION: (optional)'
    echo '   -H x.x.x.x  http server IP address (default is 127.0.0.1)'
    echo '   -p PORT     http server PORT (default is 24680)'
    echo '   -h          print this help'
    echo '   -v          print verbose info'
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
    elif [ "$1" = "-v" ]; then
        VERBOSE=1
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

if grep -q -i uos /etc/os-release; then
    . ./tool/WebUos.sh
else
    . ./tool/WebDeepin.sh
fi
