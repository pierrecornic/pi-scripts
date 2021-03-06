## Data Management

```
cd arduino/data-management
npm install
node serialportreader-v2.js
```

Wifi setup on the Pi
--------------------
!Warning! iw- commands don't support WPA (which is now the default wifi
encryption). So you must use wpa_supplicant.
!WARNING! If you connect a keyboard and a mouse to the Pi, the USB hub can't
provide enough power and the wifi dongle becomes unstable...


1. Make sure that wlan0 exists:
	iwconfig

2. If you want to check that you detect wifi networks:
	sudo iwlist scan
	sudo iwlist scan | grep my_network_name

3. Configure you ssid (network name) and password by editing the configuration
   file (with your favorite editor).
```
vim /etc/wpa_supplicant/wpa_supplicant.conf
	network={
	    ssid="ssid_name"
	    psk="password"
	}
```

4. Launch the wpa_supplicant daemon
	sudo wpa_supplicant -B -iwlan0 -c/etc/wpa_supplicant/wpa_supplicant.conf


5. Obtain an IP address
	sudo dhclient wlan0

6. ifconfig should display an IP for wlan0

7. ping google.com

8. Make it connect at boot time: edit /etc/network/interfaces to add

	auto wlan0
	allow-hotplug wlan0
	iface wlan0 inet dhcp
	wpa-conf /etc/wpa_supplicant/wpa_supplicant.conf


sources:
https://askubuntu.com/questions/138472/how-do-i-connect-to-a-wpa-wifi-network-using-the-command-line/138476#138476
http://weworkweplay.com/play/automatically-connect-a-raspberry-pi-to-a-wifi-network/


Arduino-pi connection
---------------------
The arduino's USB wire is simply plugged to the pi.
This way, the pi can easily reflash the arduino's program (aka sketch),
and exchange data through the serial port.

1. On the pi, download the arduino IDE (for Linux ARM) and install it.
2. Install the necessary libraries:
If you have a display on the pi (or vncserver), open the IDE and use the library manager.
If not, locate the libraries' github repos, clone them, and put simlinks in arduino/libraries/.
3. To build and flash the arduino from the pi:
cd arduino-1.8.5; ./arduino --port /dev/ttyACM0 --board  arduino:avr:uno --upload ../pi-scripts/arduino/sensors-management/Weather_1/Weather_1.ino
Warning: make sure that nobody else is accessing /dev/ttyACM0 otherwise the upload will fail.

Network issue
-------------

When the Pi boots with the ethernet unplugged, it can't be ping'ed.
It's caused by a wrong config of /etc/network/interfaces: if eth0 is
configured with a static IP, it's configured at boot time, and a route
is added, regardless of the wire being plugged or not.

The right way is to configure the static IPs in /etc/dhcpd.conf instead.
