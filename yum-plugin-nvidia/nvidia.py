#!/usr/bin/python
#
# yum-plugin-nvidia - a yum plugin to prevent updating nvidia drivers
#                     on older hardware that is no longer supported by
#                     the current driver release.
#
# Copyright (C) 2015 Philip J Perry <phil@elrepo.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

from yum.plugins import TYPE_CORE
import glob
import fnmatch

requires_api_version = '2.1'
plugin_type = (TYPE_CORE,)

def init_hook(conduit):
    global nvidia_devices
    nvidia_devices = []

    for file in glob.glob('/sys/bus/*/devices/*/modalias'):
        f = open(file, 'r')
        modalias = f.read()[:-1]
        f.close()

        # find only NVIDIA display devices
        if modalias.find(':v000010DE') > 0 and modalias.find('bc03sc') > 0:
            conduit.info(2, '[nvidia]: device found: %s' % modalias)
            nvidia_devices.append('blacklist(' + modalias + ')')

    if not nvidia_devices:
        conduit.info(2, '[nvidia]: No NVIDIA display devices found')

def exclude_hook(conduit):

    def find_matches(pkg, provides, matchfor=None):
        # Skip installed packages
        if pkg.repo.id == 'installed':
            return

        for device in nvidia_devices:
            for prov in provides:
                blacklist = prov.split()[0]
                if fnmatch.fnmatch(device, blacklist):
                    conduit.info(3, '[nvidia]: device not supported: %s' % blacklist)
                    conduit.info(2, '[nvidia]: excluding %s' % pkg)
                    conduit.delPackage(pkg)
                    return


    if nvidia_devices:
        conduit._base.searchPackageProvides(['blacklist(pci:v000010DEd0000*bc03sc*)', ], \
                                    callback=find_matches, callback_has_matchfor=True)
