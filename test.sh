#!/bin/sh

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <on|off|get|devices>"
    exit 1
fi

command="$1"

case "$command" in
    on)
        echo "Toggle on"
        sudo ubus call espcommd on '{"port": "/dev/ttyUSB0", "pin": 5}'
        sudo ubus call espcommd on '{"port": "/dev/ttyUSB0", "pin": 0}'
        ;;
    off)
        echo "Toggle off"
        sudo ubus call espcommd off '{"port": "/dev/ttyUSB0", "pin": 5}'
        sudo ubus call espcommd off '{"port": "/dev/ttyUSB0", "pin": 0}'
        ;;
    get)
        echo "Read sensor"
        sudo ubus call espcommd get '{"port": "/dev/ttyUSB0", "pin": 4, "sensor": "dht", "model": "dht11"}'
        ;;
    devices)
        echo "List devices"
        sudo ubus call espcommd devices
        ;;
    *)
        echo "Invalid command. Please use on, off, get, or devices."
        exit 1
        ;;
esac
