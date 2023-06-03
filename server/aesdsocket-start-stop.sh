#!/bin/sh

# Description: start/stop init script for the aesd socket server application assignment

do_start() {
    AESDSOCKET_ARGS="$AESDSOCKET_ARGS -d"
    printf "Starting aesdsocket as daemon..."
    umask 077
    
    start-stop-daemon --start --quiet --name aesdsocket --exec /usr/bin/aesdsocket -- $AESDSOCKET_ARGS
    [ $? = 0 ] && echo "OK" || echo "FAIL"
}

do_stop() {
    printf "Stopping aesdsocket..."
    start-stop-daemon --stop --quit --name aesdsocket --signal SIGTERM
    [ $? = 0 ] && echo "OK" || echo "FAIL"
}

case "$1" in
    start)
        do_start
        ;;
    stop)
        do_stop
        ;;
    restart)
        do_stop
        do_start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
esac

exit 0
        

