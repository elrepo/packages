#!/bin/bash -e
echo "KERNEL==\"a3818_[0-9]\", MODE=\"0666\"" > /etc/udev/rules.d/10-CAEN-A3818.rules
echo "KERNEL==\"a3818_[0-9][0-9]\", MODE=\"0666\"" >> /etc/udev/rules.d/10-CAEN-A3818.rules
udevadm control --reload-rules