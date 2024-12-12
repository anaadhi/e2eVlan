#!/bin/bash
sudo ip tuntap add dev tap0 mode tap
sudo ip addr add 10.1.1.101/24 dev tap0
sudo ip link set tap0 up