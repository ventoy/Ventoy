#!/bin/sh

[% IF lsb -%]
### BEGIN INIT INFO
# Provides:          vblade
# Required-Start:    $remote_fs $syslog $network
# Required-Stop:     $remote_fs $syslog $network
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: vblade exports
# Description:       Manage all vlbade exports defined in
#                    /etc/vblade.conf.d/
### END INIT INFO

[% END -%]
PATH=/sbin:/usr/sbin:/bin:/usr/bin
DESC="vblade export"
NAME=vblade
VBLADE="/usr/sbin/$NAME"
[% IF control == 'ssd' -%]
[% PERL -%]die ('control=ssd cannot be used as  long as vblade as no pidfile support');[% END -%]
[% ELSIF control == 'daemon' -%]
DAEMON=/usr/bin/daemon
[% END -%]
IONICE=/usr/bin/ionice
PIDDIR="/var/run/vblade/"

[ -x "$VBLADE" ] || exit 0
[% IF control == 'daemon' -%]
[ -x "$DAEMON" ] || exit 0
[% END -%]

mkdir -p "$PIDDIR"

[% IF lsb -%]
# Load the VERBOSE setting and other rcS variables
. /lib/init/vars.sh

# Define LSB functions
. /lib/lsb/init-functions
[% ELSE -%]
# Emulation of LSB functions
VERBOSE=1
log_daemon_msg () {
    printf '%s ' "$@"
}
log_end_msg () {
    local CODE="$1"
    if [ "$CODE" -eq 0 ] ; then
        echo '.'
    else
        echo 'failed!'
    fi
}
[% END -%]

# Start a vblade instance
#
# Return
#   0 if daemon has been started
#   1 if daemon was already running
#   2 if daemon could not be started
do_start () {
    local INSTANCE="$1"
    local CONFIG="$2"

    sh -n "$CONFIG" 2>/dev/null || return 2

    shelf=
    slot=
    filename=
    netif=
    options=
    ionice=

    . "$CONFIG"

    [ "$netif" ] || return 2
    [ "$shelf" ] || return 2
    [ "$slot" ] || return 2
    [ "$filename" ] || return 2

    if [ "$ionice" ] ; then
        if [ -x "$IONICE" ] ; then
            ionice="$IONICE $ionice"
        else
            ionice=
        fi
    fi

[% IF control == 'ssd' -%]
    local PIDFILE="$PIDDIR/$INSTANCE.pid"
    start-stop-daemon --start --quiet \
        --pidfile "$PIDFILE" --exec "$VBLADE" --test > /dev/null \
        || return 1
    start-stop-daemon --start --quiet \
        --pidfile "$PIDFILE" \
        --exec $ionice "$VBLADE" -- \
        $shelf $slot $netif $filename $options \
        || return 2
[% ELSIF control == 'daemon' -%]
    "$DAEMON" \
        --running \
        --name "$INSTANCE" \
        --pidfiles "$PIDDIR" \
        && return 1
    $ionice "$DAEMON" \
        --respawn \
        --name "$INSTANCE" \
        --pidfiles "$PIDDIR" \
        --output daemon.notice \
        --stdout daemon.notice \
        --stderr daemon.err -- \
        $VBLADE $options $shelf $slot $netif $filename || return 2
[% END -%]
}

# Stop a vblade instance
#
# Return
#   0 if daemon has been stopped
#   1 if daemon was already stopped
#   2 if daemon could not be stopped
#   other if a failure occurred
do_stop () {
    local INSTANCE="$1"

[% IF control == 'ssd' -%]
    local PIDFILE="$PIDDIR/$INSTANCE.pid"
    start-stop-daemon --stop --quiet \
        --retry=TERM/30/KILL/5 --pidfile "$PIDFILE" --name "$NAME"
    RETVAL="$?"
    [ "$RETVAL" = 2 ] && return 2
    # Many daemons don't delete their pidfiles when they exit.
    rm -f "$PIDFILE"
    return "$RETVAL"
[% ELSIF control == 'daemon' -%]
    "$DAEMON" \
        --running \
        --name "$INSTANCE" \
        --pidfiles "$PIDDIR" || return 1
    "$DAEMON" \
        --stop \
        --name "$INSTANCE" \
        --pidfiles "$PIDDIR" \
        --stop || return 2
    # Wait until the process is gone
    for i in $(seq 1 10) ; do
        "$DAEMON" \
            --running \
            --name "$INSTANCE" \
            --pidfiles "$PIDDIR" || return 0
    done
    return 2
[% END -%]
}

EXIT=0

do_action () {
    local CONFIG="$1"

    INSTANCE="$(basename "${CONFIG%%.conf}")"

    case "$ACTION" in
        start)
            [ "$VERBOSE" != no ] && log_daemon_msg "Starting $DESC" "$INSTANCE"
            do_start "$INSTANCE" "$CONFIG"
            case "$?" in
                0|1)    [ "$VERBOSE" != no ] && log_end_msg 0 ;;
                2)      [ "$VERBOSE" != no ] && log_end_msg 1 ;;
            esac
            ;;
        stop)
            [ "$VERBOSE" != no ] && log_daemon_msg "Stopping $DESC" "$INSTANCE"
            do_stop "$INSTANCE"
            case "$?" in
                0|1)    [ "$VERBOSE" != no ] && log_end_msg 0 ;;
                2)      [ "$VERBOSE" != no ] && log_end_msg 1 ;;
            esac
            ;;
        status)
[% IF lsb -%]
            status_of_proc -p "$PIDDIR/$INSTANCE.pid" "$VBLADE" "vblade instance $INSTANCE" || EXIT=$?
[% ELSE -%]
            if "$DAEMON" \
                --running \
                --name "$INSTANCE" \
                --pidfiles "$PIDDIR"
            then
                echo "$DESC instance $INSTANCE is running"
            else
                echo "$DESC instance $INSTANCE is not running"
                EXIT=1
            fi
[% END -%]
            ;;
        restart|force-reload)
            log_daemon_msg "Restarting $DESC" "$INSTANCE"
            do_stop "$INSTANCE"
            case "$?" in
                0|1)
                    do_start "$INSTANCE" "$CONFIG"
                    case "$?" in
                        0)  log_end_msg 0 ;;
                        *)
                            # Old process is still running or
                            # failed to start
                            log_end_msg 1 ;;
                    esac
                    ;;
                *)
                    # Failed to stop
                    log_end_msg 1
                    ;;
                esac
            ;;
        *)
            echo "Usage: /etc/init.d/vblade {start|stop|status|restart|force-reload} [<export> ...]" >&2
            exit 3
            ;;
    esac
}


ACTION="$1"
shift

if [ "$1" ] ; then
    while [ "$1" ] ; do
        CONFIG="/etc/vblade.conf.d/$1.conf"
        if [ -f "$CONFIG" ] ; then
            do_action "$CONFIG"
        fi
        shift
    done
else
    for CONFIG in /etc/vblade.conf.d/*.conf ; do
        if [ -f "$CONFIG" ] ; then
            do_action "$CONFIG"
        fi
    done
fi

exit $EXIT
