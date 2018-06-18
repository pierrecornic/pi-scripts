#!/bin/bash
SERVERIP=192.168.1.1
# Reboot wifi if 3 pings unsucessful
ping -c 3 $SERVERIP > /dev/null 2>&1
if [ $? -ne 0 ]
then
	echo `date` "Wifi down"
	/sbin/ifconfig wlan0 down >> /var/log/wifi.log 2>&1
	sleep 2
	/sbin/ifconfig wlan0 up >> /var/log/wifi.log 2>&1
	echo `date` "Wifi up"
fi
