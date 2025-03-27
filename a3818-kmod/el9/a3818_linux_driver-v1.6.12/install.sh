#!/bin/bash -e
VERSION=1.6.12

sed -e "s/\${version}/$VERSION/" dkms.conf.in > dkms.conf

# Remove module if already loaded
rmmod a3818 2> /dev/null || true

# If the same version is already installed, it will be overwritten
dkms uninstall a3818/$VERSION -q || true
dkms remove a3818/$VERSION -q --all || true

cp -R . /usr/src/a3818-$VERSION
dkms add a3818/$VERSION
dkms build a3818/$VERSION
dkms install a3818/$VERSION --force

# Reload the module
modprobe a3818
