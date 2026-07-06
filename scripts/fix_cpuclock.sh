#!/bin/bash

# To desativate the CPU boost
echo 0 | sudo tee /sys/devices/system/cpu/cpufreq/boost

# To lock the CPU frequency
sudo cpupower -c all frequency-set -d 2.2GHz -u 2.2GHz
sudo cpupower -c all frequency-set -g performance

cat /proc/cpuinfo | grep MHz
