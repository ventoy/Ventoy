#!/bin/sh

print_usage() {    
    echo 'Usage:  VentoyWeb.sh [ OPTION ]'   
    echo '  OPTION: (optional)'
    echo '   -H x.x.x.x  http server IP address (default is 127.0.0.1)'
    echo '   -p PORT     http server PORT (default is 24680)'
    echo "   -n          don't start web browser"
    echo '   -h          print this help'
    echo ''
}

print_err() {
    echo ""
    echo "$*"
    echo ""
}

check_option() {
    app="$1"
    $app --help 2>&1 | grep -q "$2"
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

chromium_proc() {
    app="$1"
    
    url="http://${HOST}:${PORT}/index.html"
    
    if check_option "$app" '[-][-]app='; then
        su $VUSER -c "$app --app=$url >> $LOGFILE 2>&1"
    elif check_option "$app" '[-][-]new[-]window='; then
        su $VUSER -c "$app --new-window $url >> $LOGFILE 2>&1"
    else
        su $VUSER -c "$app $url >> $LOGFILE 2>&1"
    fi
}

uid=$(id -u)
if [ $uid -ne 0 ]; then
    print_err "Please use sudo or run the script as root."
    exit 1
fi

OLDDIR=$(pwd)

if uname -a | egrep -q 'aarch64|arm64'; then
    TOOLDIR=aarch64
elif uname -a | egrep -q 'x86_64|amd64'; then
    TOOLDIR=x86_64
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
    elif [ "$1" = "-n" ]; then
        NOWEB=1
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
#delete the log.txt if it's more than 8MB
if [ -f $LOGFILE ]; then
    logsize=$(stat -c '%s' $LOGFILE)
    if [ $logsize -gt 8388608 ]; then
        rm -f $LOGFILE
        su $VUSER -c "touch $LOGFILE"
    fi
else
    su $VUSER -c "touch $LOGFILE"
fi



if [ -f ./tool/$TOOLDIR/V2DServer.xz ]; then
    xz -d ./tool/$TOOLDIR/V2DServer.xz
    chmod +x ./tool/$TOOLDIR/V2DServer
fi

V2DServer "$HOST" "$PORT" &

vtVer=$(cat ventoy/version)
echo ""
echo "=================================================================="
echo "  Ventoy Server $vtVer is running at http://${HOST}:${PORT} ..."
echo "=================================================================="
echo ""
echo "################ Press Ctrl + C to exit ######################"
echo ""

if [ "$NOWEB" = "1" ]; then
    echo "Please open your web browser and visit http://${HOST}:${PORT}"
else
    if which -a google-chrome-stable >> $LOGFILE 2>&1; then    
        chromium_proc google-chrome-stable
    elif which -a google-chrome >> $LOGFILE 2>&1; then    
        chromium_proc google-chrome
    elif which -a chrome >> $LOGFILE 2>&1; then    
        chromium_proc chrome
    elif which -a browser >> $LOGFILE 2>&1; then        
        chromium_proc browser        
    elif which -a firefox >> $LOGFILE 2>&1; then
        su $VUSER -c "firefox --no-remote \"http://${HOST}:${PORT}/index.html\""
    else
        echo "Please open your web browser and visit http://${HOST}:${PORT}"
    fi
fi

if ps -ef | grep "V2DServer.*$HOST.*$PORT" | grep -q -v grep; then
    echo ""
else
    print_err "Ventoy Server Error! Please check log.txt."
fi

wait $!


if [ -n "$OLDDIR" ]; then 
    CURDIR=$(pwd)
    if [ "$CURDIR" != "$OLDDIR" ]; then
        cd "$OLDDIR"
    fi
fi
