#!/bin/bash -e
VERSION=1.0.5

sed -e "s/\${version}/$VERSION/" dkms.conf.in > dkms.conf

# Remove module if already loaded
rmmod a5818 2> /dev/null || true

# If the same version is already installed, it will be overwritten
dkms uninstall a5818/$VERSION -q || true
dkms remove a5818/$VERSION -q --all || true

cp -R . /usr/src/a5818-$VERSION
dkms add a5818/$VERSION
dkms build a5818/$VERSION
dkms install a5818/$VERSION --force

# Reload the module
modprobe a5818
