Network issue
-------------

When the Pi boots with the ethernet unplugged, it can't be ping'ed.
It's caused by a wrong config of /etc/network/interfaces: if eth0 is
configured with a static IP, it's configured at boot time, and a route
is added, regardless of the wire being plugged or not.

The right way is to configure the static IPs in /etc/dhcpd.conf instead.
