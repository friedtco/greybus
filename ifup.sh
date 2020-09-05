#!/bin/sh

# get the phy of the first wpan device
PHY="$(iwpan list | grep "^wpan_phy " | head -n 1 | awk '{print $2}')"
CHANNEL=26
PAN_ID=0xabcd

WPAN=wpan0
LOWPAN=lowpan0
ADDR=2001:db8::2/64

sudo ip link set ${WPAN} down
sudo ip link set ${LOWPAN} down
sudo iwpan phy ${PHY} set channel 0 ${CHANNEL}
sudo iwpan dev ${WPAN} set pan_id ${PAN_ID}
sudo ip link add link ${WPAN} name ${LOWPAN} type lowpan
sudo ip link set ${WPAN} up
sudo ip link set ${LOWPAN} up
sudo ip -6 addr add ${ADDR} dev ${LOWPAN}
