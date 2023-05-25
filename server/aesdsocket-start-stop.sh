#! /bin/bash

# Description: start/stop init script for the aesd socket server application assignment

do_start() {
    start-stop-daemon --start --startas /usr/bin/aesdsocket -- -d
}

do_stop() {
    start-stop-daemon --stop --name aesdsocket --signal SIGTERM
}

do_status() {
    start-stop-daemon --status --name aesdsocket && exit_status=$? || exit_status=$?
    case "$exit_status" in
        0)
            echo "aesdsocket is running."
            ;;
        3)
            echo "aesdsocket is not running."
            ;;
        4)
            echo "Unable to determine aesdsocket status."
            ;;
    esac
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
    status)
        do_status
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
esac

exit 0
        

