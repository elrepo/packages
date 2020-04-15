#!/usr/bin/python
#
# yum-plugin-elrepo - a yum plugin to remove kmod packages from the
#                     yum transaction set which require kernels which
#                     are not yet available.
#
# Copyright (C) 2018 Philip J Perry <phil@elrepo.org>
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
import fnmatch

requires_api_version = '2.6'
plugin_type = (TYPE_CORE,)

def init_hook(conduit):
    global kernels
    kernels = []

def exclude_hook(conduit):

    # get installed kernels
    instpkgs = conduit.getRpmDB().returnPackages()
    for instpkg in instpkgs:
        if instpkg.name == "kernel":
            kernels.append('kernel-modules >= ' + instpkg.version + '-' + instpkg.release + '.' + instpkg.arch)
            conduit.info(4, '[elrepo]: found installed kernel: %s' % instpkg)

    # get available kernels
    pkgs = conduit.getPackages()
    for pkg in pkgs:
        if pkg.name == "kernel":
            kernels.append('kernel-modules >= ' + pkg.version + '-' + pkg.release + '.' + pkg.arch)
            conduit.info(4, '[elrepo]: found kernel: %s' % pkg)

    if not kernels:
        conduit.info(4, '[elrepo]: ERROR, no kernels found')
        return

    def find_matches(kmod, requires, matchfor=None):

        # Skip installed packages
        if kmod.repo.id == "installed":
            return

        # Skip non-elrepo packages
        if not (kmod.release).endswith(".elrepo"):
            return

        for req in requires:
            for kernel in kernels:
                if fnmatch.fnmatch(kernel, req):
                    return
        # else no matching kernel, excluding package
        conduit.info(4, '[elrepo]: excluding package: %s' % kmod)
        conduit.delPackage(kmod)

        # if kmod-nvidia, handle matching nvidia-x11-drv packages
        if (kmod.name).startswith("kmod-nvidia"):
            for pkg in pkgs:
                if (pkg.name).startswith("nvidia-x11-drv"):
                    if kmod.version == pkg.version and kmod.release == pkg.release:
                        conduit.info(4, '[elrepo]: excluding package: %s' % pkg)
                        conduit.delPackage(pkg)


    conduit._base.searchPackageProvides(['kernel-modules >= *', ], callback=find_matches, callback_has_matchfor=True)
