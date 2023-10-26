# FERS DT5200
## implementation with EUDAQ

# How to setup multiple IP on the same internet interface (PC)

NOTE: both FERS and secondary PC IP MUST be like 192.168.50.x, that is, local network type
NOTE: these instructions work on ubuntu 20.04
NOTE: the primary IP of the PC in the following instructions is identical to the one automatically assigned by DHCP by the internet switch, yours may vary. You can check it with
ifconfig
look for the inet of ethernet interface (eth0, eno1, ..., depending on the version of the OS)

## How to temporarily add the secondary IP (in this example, 192.168.50.100) to eno1 interface:

sudo ifconfig eno1:1 192.168.50.100/24 netmask 255.255.255.0 up

## How to permanently add the secondary IP (on ubuntu)
In this example, the PC IP address is chosen to be 193.206.209.68, YMMV.

1) modify /etc/netplan/01-network-manager-all.yaml 
network:
  version: 2
  renderer: NetworkManager
  ethernets:
    eno1:
       addresses:
         - 193.206.209.68/24
         - 192.168.50.100/24
       gateway4: 192.168.1.1
       nameservers:
          addresses: [1.1.1.1, 1.0.0.1]
       optional: true

2) sudo netplan generate
3) sudo netplan apply
4) reboot
5) check that the Ethernet interface has two IP via ifconfig

## the current IP should be automatically chosen depending on the presence of the cable that link the switch to the external network
When local IP is active (external cable is unpluggled) the FERS should be visible;
Vice-versa, when the external net is plugged, the FERS should be unreachable, but the PC should be able to access internet
