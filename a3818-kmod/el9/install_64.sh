#!/bin/bash

cp -R . /usr/src/a3818-1.6.8
dkms add -m a3818 -v 1.6.8
dkms build -m a3818 -v 1.6.8
dkms install -m a3818 -v 1.6.8
insmod /lib/modules/`uname -r`/updates/dkms/a3818.ko
