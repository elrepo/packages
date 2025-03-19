#!/bin/bash -e
rm /etc/udev/rules.d/10-CAEN-A3818.rules
udevadm control --reload-rules
