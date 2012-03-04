#define gitdate 20110106

Summary: Direct Rendering Manager runtime library
Name: libdrm
Version: 2.4.31
Release: 3%{?dist}
License: MIT
Group: System Environment/Libraries
URL: http://dri.sourceforge.net
Source0: http://dri.freedesktop.org/libdrm/%{name}-%{version}.tar.bz2
#Source0: %{name}-%{gitdate}.tar.bz2
Source1: make-git-snapshot.sh

Requires: udev
Requires: kernel >= 2.6.29.1-52.fc11

BuildRequires: pkgconfig automake autoconf libtool
BuildRequires: kernel-headers >= 2.6.29-0.145.rc6.fc11
BuildRequires: libxcb-devel
BuildRequires: libudev-devel
BuildRequires: libatomic_ops-devel
BuildRequires: libpciaccess-devel

Source2: 91-drm-modeset.rules

# hardcode the 666 instead of 660 for device nodes
Patch3: libdrm-make-dri-perms-okay.patch
# remove backwards compat not needed on Fedora
Patch4: libdrm-2.4.0-no-bc.patch
# make rule to print the list of test programs
Patch5: libdrm-2.4.25-check-programs.patch

%description
Direct Rendering Manager runtime library

%package devel
Summary: Direct Rendering Manager development package
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}
Requires: kernel-headers >= 2.6.27-0.144.rc0.git2.fc10
Requires: pkgconfig

%description devel
Direct Rendering Manager development package

%package -n drm-utils
Summary: Direct Rendering Manager utilities
Group: Development/Tools

%description -n drm-utils
Utility programs for the kernel DRM interface.  Will void your warranty.

%prep
#setup -q -n %{name}-%{gitdate}
%setup -q
%patch3 -p1 -b .forceperms
%patch4 -p1 -b .no-bc
%patch5 -p1 -b .check

%build
autoreconf -v --install || exit 1
%configure --enable-udev --enable-nouveau-experimental-api --enable-libkms
make %{?_smp_mflags}
pushd tests
make %{?smp_mflags} `make check-programs`
popd

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
pushd tests
mkdir -p $RPM_BUILD_ROOT%{_bindir}
for foo in $(make check-programs) ; do
 install -m 0755 .libs/$foo $RPM_BUILD_ROOT%{_bindir}
done
popd
# SUBDIRS=libdrm
mkdir -p $RPM_BUILD_ROOT/lib/udev/rules.d/
install -m 0644 %{SOURCE2} $RPM_BUILD_ROOT/lib/udev/rules.d/

# NOTE: We intentionally don't ship *.la files
find $RPM_BUILD_ROOT -type f -name '*.la' | xargs rm -f -- || :
for i in r300_reg.h via_3d_reg.h
do
rm -f $RPM_BUILD_ROOT/usr/include/libdrm/$i
done

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc README
%{_libdir}/libdrm.so.2
%{_libdir}/libdrm.so.2.4.0
%{_libdir}/libkms.so.1
%{_libdir}/libkms.so.1.0.0
%ifarch %{ix86} x86_64 ia64
%{_libdir}/libdrm_intel.so.1
%{_libdir}/libdrm_intel.so.1.0.0
%endif
%{_libdir}/libdrm_radeon.so.1
%{_libdir}/libdrm_radeon.so.1.0.0
%{_libdir}/libdrm_nouveau.so.1
%{_libdir}/libdrm_nouveau.so.1.0.0
/lib/udev/rules.d/91-drm-modeset.rules

%files -n drm-utils
%defattr(-,root,root,-)
%{_bindir}/dristat
%{_bindir}/drmstat
%{_bindir}/gem_basic
%{_bindir}/gem_flink
%{_bindir}/gem_mmap
%{_bindir}/gem_readwrite
%{_bindir}/getclient
%{_bindir}/getstats
%{_bindir}/getversion
%{_bindir}/name_from_fd
%{_bindir}/openclose
%{_bindir}/setversion
%{_bindir}/updatedraw

%files devel
%defattr(-,root,root,-)
# FIXME should be in drm/ too
%{_includedir}/xf86drm.h
%{_includedir}/xf86drmMode.h
%{_includedir}/libkms/libkms.h
%{_includedir}/libdrm/drm.h
%{_includedir}/libdrm/drm_fourcc.h
%{_includedir}/libdrm/drm_mode.h
%{_includedir}/libdrm/drm_sarea.h
%ifarch %{ix86} x86_64 ia64
%{_includedir}/libdrm/intel_bufmgr.h
%{_includedir}/libdrm/intel_debug.h
%endif
%{_includedir}/libdrm/radeon_bo.h
%{_includedir}/libdrm/radeon_bo_gem.h
%{_includedir}/libdrm/radeon_bo_int.h
%{_includedir}/libdrm/radeon_cs.h
%{_includedir}/libdrm/radeon_cs_gem.h
%{_includedir}/libdrm/radeon_cs_int.h
%{_includedir}/libdrm/radeon_surface.h
%{_includedir}/libdrm/r600_pci_ids.h
%{_includedir}/libdrm/nouveau_drmif.h
%{_includedir}/libdrm/*_drm.h
%dir %{_includedir}/nouveau
%{_includedir}/nouveau/nouveau_*.h
%{_includedir}/nouveau/nv*_pushbuf.h
%{_libdir}/libdrm.so
%{_libdir}/libkms.so
%ifarch %{ix86} x86_64 ia64
%{_libdir}/libdrm_intel.so
%endif
%{_libdir}/libdrm_radeon.so
%{_libdir}/libdrm_nouveau.so
%{_libdir}/pkgconfig/libdrm.pc
%ifarch %{ix86} x86_64 ia64
%{_libdir}/pkgconfig/libdrm_intel.pc
%endif
%{_libdir}/pkgconfig/libdrm_radeon.pc
%{_libdir}/pkgconfig/libdrm_nouveau.pc
%{_libdir}/pkgconfig/libkms.pc

%changelog
* Sun Feb 26 2012 Phil Schaffner <pschaff2@verison.net> 2.4.31-3
- Build for EL6

* Wed Feb 22 2012 Adam Jackson <ajax@redhat.com> 2.4.31-3
- Fix build on non-Intel arches

* Tue Feb 07 2012 Jerome Glisse <jglisse@redhat.com> 2.4.31-2
- Fix missing header file

* Tue Feb 07 2012 Jerome Glisse <jglisse@redhat.com> 2.4.31-1
- upstream 2.4.31 release

* Fri Jan 20 2012 Dave Airlie <airlied@redhat.com> 2.4.30-1
- upstream 2.4.30 release

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.4.27-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Fri Nov 11 2011 Adam Jackson <ajax@redhat.com> 2.4.27-2
- Fix typo in udev rule

* Tue Nov 01 2011 Adam Jackson <ajax@redhat.com> 2.4.27-1
- libdrm 2.4.27

* Wed Oct 26 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.4.26-4
- Rebuilt for glibc bug#747377

* Tue Oct 25 2011 Adam Jackson <ajax@redhat.com> 2.4.26-3
- Fix udev rule matching and install location (#748205)

* Fri Oct 21 2011 Dave Airlie <airlied@redhat.com> 2.4.26-2
- fix perms on control node in udev rule

* Mon Jun 06 2011 Adam Jackson <ajax@redhat.com> 2.4.26-1
- libdrm 2.4.26 (#711038)

* Wed Apr 20 2011 Bill Nottingham <notting@redhat.com> 2.4.25-3
- fix drm-utils subpackage

* Mon Apr 18 2011 Adam Jackson <ajax@redhat.com> 2.4.25-2
- Add subpackage for the drm utilities

* Mon Apr 11 2011 Dave Airlie <airlied@redhat.com> 2.4.25-1
- libdrm 2.4.25

* Wed Mar 09 2011 Adam Jackson <ajax@redhat.com> 2.4.24-1
- libdrm 2.4.24

* Mon Feb 07 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.4.24-0.2.20110106
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Thu Jan 06 2011 Ben Skeggs <bskeggs@redhat.com> 2.4.24-0.1.20110106
- Today's git snapshot

* Mon Dec 13 2010 Adam Jackson <ajax@redhat.com> 2.4.23-2
- libdrm 2.4.23

* Sun Dec 12 2010 Dave Airlie <airlied@redhat.com> 2.4.23-1.20101212
- 2.4.23 release snapshot

* Tue Nov 23 2010 Adam Jackson <ajax@redhat.com> 2.4.23-0.1.20101123
- Today's git snapshot

* Mon Nov 08 2010 Adam Jackson <ajax@redhat.com> 2.4.23-0.1.20101108
- Today's git snapshot

* Tue Oct 19 2010 Adam Jackson <ajax@redhat.com> 2.4.22-0.1.20101019
- Today's git snapshot

* Wed Oct 06 2010 Adam Jackson <ajax@redhat.com> 2.4.22-1
- libdrm 2.4.22

* Mon Jul 05 2010 Dave Airlie <airlied@redhat.com> 2.4.21-3
- pull in latest git changes

* Fri Jun 25 2010 Adam Jackson <ajax@redhat.com> 2.4.21-2
- re-enable libkms

* Fri Jun 25 2010 Adam Jackson <ajax@redhat.com> 2.4.21-1
- libdrm 2.4.21

* Thu Apr 08 2010 Dave Airlie <airlied@redhat.com> 2.4.20-1
- upstream release - includes all header file fixes

* Fri Mar 26 2010 Dave Airlie <airlied@redhat.com> 2.4.19-2
- fix up include files now they don't conflict with kernel.

* Fri Mar 19 2010 Ben Skeggs <bskeggs@redhat.com> 2.4.19-1
- upstream release 2.4.19 + fixes up until git c1c8bff

* Fri Feb 19 2010 Ben Skeggs <bskeggs@redhat.com> 2.4.18-1
- upstream release 2.4.18

* Wed Feb 17 2010 Ben Skeggs <bskeggs@redhat.com> 2.4.18-0.1
- rebase to pre-snapshot of 2.4.18

* Fri Feb 12 2010 Adam Jackson <ajax@redhat.com> 2.4.17-3
- Own %%{_includedir}/nouveau (#561317)

* Wed Feb 03 2010 Dave Airlie <airlied@redhat.com> 2.4.17-2
- update to git master

* Mon Dec 21 2009 Dave Airlie <airlied@redhat.com> 2.4.17-1
- upstream released 2.4.17

* Mon Dec 21 2009 Dave Airlie <airlied@redhat.com> 2.4.17-0.1
- new radeon API from upstream rebase

* Tue Dec 01 2009 Dave Airlie <airlied@redhat.com> 2.4.16-0.1
- rebase to pre-snapshot of 2.4.16

* Sat Nov 28 2009 Dave Airlie <airlied@redhat.com> 2.4.15-6
- add new upstream API for drivers.

* Fri Nov 20 2009 Dave Airlie <airlied@redhat.com> 2.4.15-5
- update radeon API to upstream fixes

* Thu Nov 05 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.15-4
- nouveau: improve reloc API to allow better error handling

* Wed Nov 04 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.15-3
- nouveau: drop rendering on floor rather than asserting if flush fails

* Tue Oct 27 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.15-2
- nouveau: retry pushbuf ioctl if interrupted by signal

* Fri Oct 09 2009 Dave Airlie <airlied@redhat.com> 2.4.15-1
- rebase to latest upstream release

* Fri Sep 25 2009 Dave Airlie <airlied@redhat.com> 2.4.14-1
- rebase to latest upstream release - drop carried patches

* Thu Sep 10 2009 Kristian Høgsberg <krh@redhat.com> - 2.4.12-0.10
- Pull in intel bo busy.

* Wed Aug 26 2009 Dave Airlie <airlied@redhat.com> 2.4.12-0.9
- pull in radeon bo busy

* Thu Aug 20 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.12-0.8
- rebase to new libdrm snapshot

* Thu Aug 06 2009 Dave Airlie <airlied@redhat.com> 2.4.12-0.7
- rebase to new libdrm snapshot

* Wed Jul 29 2009 Kristian Høgsberg <krh@redhat.com> - 2.4.12-0.6
- Add libdrm support for KMS pageflip ioctl.

* Tue Jul 28 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.12-0.4
- rebase onto git snapshot for new nouveau interface support

* Fri Jul 24 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.4.12-0.3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Fri Jul 17 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.12-0.2
- rebase onto git snapshot

* Mon Jun 22 2009 Dave Airlie <airlied@redhat.com> 2.4.12-0.1
- rebase onto git snapshot - remove radeon patch in master now

* Mon Jun  8 2009 Kristian Høgsberg <krh@redhat.com> - 2.4.11-0
- Bump to 2.4.11.

* Fri Apr 17 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.6-6
- nouveau: post writes to pushbuf before incrementing PUT

* Thu Apr 16 2009 Dave Airlie <airlied@redhat.com> 2.4.6-5
- libdrm-radeon: fix wait idle

* Sat Apr 11 2009 Dave Airlie <airlied@redhat.com> 2.4.6-4
- libdrm-2.4.7-revert-bong.patch - revert connector "speedups"

* Tue Apr  7 2009 Kristian Høgsberg <krh@redhat.com> - 2.4.6-3
- BuildRequire libudev-devel for test cases.

* Mon Apr  6 2009 Kristian Høgsberg <krh@redhat.com> - 2.4.6-2
- Bump to 2.4.6

* Mon Apr 06 2009 Dave Airlie <airlied@redhat.com<> 2.4.5-4
- libdrm-radeon: API busting to latest upstream
- bump kernel requires

* Thu Mar 26 2009 Adam Jackson <ajax@redhat.com> 2.4.5-3
- libdrm-intel-gtt.patch: Fix GTT maps for intel.

* Wed Mar 25 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.5-2
- pull in nouveau fixes from drm.git

* Mon Mar 23 2009 Dave Airlie <airlied@redhat.com> 2.4.5-1
- add bo naming

* Tue Feb 24 2009 Kristian Høgsberg <krh@redhat.com> - 2.4.5-0
- Update to 2.4.5, drop nouveau and intel patches, rebase radeon.

* Mon Feb 23 2009 Kristian Høgsberg <krh@redhat.com> - 2.4.4-9
- Pull in intel bufmgr changes while waiting for 2.4.5.

* Mon Feb 23 2009 Dave Airlie <airlied@redhat.com> 2.4.4-6
- don't use the CS patch need_flush

* Wed Feb 18 2009 Dave Airlie <airlied@redhat.com> 2.4.4-5
- update libdrm_radeon again

* Thu Feb 05 2009 Ben Skeggs <bskeggs@redhat.com> 2.4.4-4
- nouveau: pull in updates from upstream

* Thu Feb 05 2009 Dave Airlie <airlied@redhat.com> 2.4.4-3
- update with more libdrm/radeon upstream fixes

* Sun Feb 01 2009 Dave Airlie <airlied@redhat.com> 2.4.4-2
- update specfile with review changes

* Fri Jan 30 2009 Dave Airlie <airlied@redhat.com> 2.4.4-1
- rebase to 2.4.4

* Mon Dec 22 2008 Dave Airlie <airlied@redhat.com> 2.4.3-0.3
- radeon: make library name correct

* Mon Dec 22 2008 Dave Airlie <airlied@redhat.com> 2.4.3-0.2
- radeon: update with fixes for reloc size

* Fri Dec 19 2008 Dave Airlie <airlied@redhat.com> 2.4.3-0.1
- libdrm: update to upstream master + add radeon patches from modesetting-gem

* Tue Sep 30 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.21
- move intel bufmgr code around - update patches

* Tue Sep 09 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.20
- add gtt mapping for intel modesetting

* Thu Aug 14 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.19
- add back modesetting support - this is a snapshot from modesetting-gem
- any bugs are in the other packages that fail to build

* Mon Aug 11 2008 Adam Jackson <ajax@redhat.com> 2.4.0-0.18
- Today's git snap.

* Sun Aug 10 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.17
- attempt to fix race with udev by just waiting for udev

* Fri Aug 01 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.16
- new libdrm snapshot with modesetting for radeon interfaces

* Thu Jul 17 2008 Kristian Høgsberg <krh@redhat.com> - 2.4.0-0.15
- Avoid shared-core when doing make install so we don't install kernel
  header files.  Drop kernel header files from -devel pkg files list.

* Thu Jul 17 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.14
- kernel headers now installs somes of these files for us

* Wed Jun 18 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.13
- add modeset ctl interface fix

* Wed May 28 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.12
- add r500 support patch

* Tue Apr 29 2008 Adam Jackson <ajax@redhat.com> 2.4.0-0.11
- libdrm-2.4.0-no-bc.patch: Delete the /proc/dri BC code.  It's not needed,
  and the kernel implementation is sufficiently broken that we should avoid
  ever touching it.

* Wed Mar 19 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.10
- force libdrm to make the node perms useful to everyone 

* Fri Mar 07 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.9
- add support for new sysfs structure

* Thu Mar 06 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.8
- add modprobe.d file so i915 modesetting can be specified on kernel command
  line

* Wed Mar 05 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.7
- add udev rules for modesetting nodes.

* Wed Mar 05 2008 Dave Airlie <airlied@redhat.com> 2.4.0-0.6
- add initial modesetting headers to the mix - this API isn't stable 

* Mon Mar  3 2008 Kristian Høgsberg <krh@redhat.com> - 2.4.0-0.5
- What he said.

* Fri Feb 15 2008 Adam Jackson <ajax@redhat.com> 2.4.0-0.4
- Today's git snapshot for updated headers.

* Mon Jan 21 2008 Adam Jackson <ajax@redhat.com> 2.4.0-0.3
- libdrm-2.4.0-no-freaking-mknod.patch: Disable.  Deep voodoo.

* Thu Nov 30 2007 Dave Airlie <airlied@redhat.com> - 2.4.0-0.2
- Update to a newer upstream snapshot

* Mon Nov 12 2007 Adam Jackson <ajax@redhat.com> 2.4.0-0.1
- libdrm-2.4.0-no-freaking-mknod.patch: Don't magically mknod the device
  file, that's what udev is for.

* Thu Nov 01 2007 Dave Airlie <airlied@redhat.com> - 2.4.0-0
- Import a snapshot of what will be 2.4 upstream

* Thu Sep 20 2007 Dave Airlie <airlied@redhat.com> - 2.3.0-7
- Update nouveau patch.

* Tue Aug 21 2007 Adam Jackson <ajax@redhat.com> - 2.3.0-6
- Rebuild for build id

* Fri Mar 30 2007 Kristian Høgsberg <krh@redhat.com> - 2.3.0-5
- Update nouveau patch.

* Tue Feb 19 2007 Adam Jackson <ajax@redhat.com> 2.3.0-4
- Update nouveau patch
- Fix License tag and other rpmlint noise

* Fri Feb 02 2007 Adam Jackson <ajax@redhat.com> 2.3.0-3
- Remove ExclusiveArch.

* Mon Jan 29 2007 Adam Jackson <ajax@redhat.com> 2.3.0-2
- Change default device mode to 0666. (#221545)

* Fri Nov 17 2006 Adam Jackson <ajax@redhat.com> 2.3.0-1.fc7
- Update to 2.3.0 from upstream.
- Add nouveau userspace header.

* Wed Jul 26 2006 Kristian Høgsberg <krh@redhat.com> - 2.0.2-3.fc6
- Build for rawhide.

* Wed Jul 26 2006 Kristian Høgsberg <krh@redhat.com> - 2.0.2-2.fc5.aiglx
- Build for fc5 aiglx repo.

* Wed Jul 12 2006 Jesse Keating <jkeating@redhat.com> 2.0.2-2.1
- rebuild

* Tue Jun 27 2006 Adam Jackson <ajackson@redhat.com> 2.0.2-2
- Bump to 2.0.2 for header updates.  Fix BuildRequires.  Minor spec cleanups. 

* Mon Jun 09 2006 Mike A. Harris <mharris@redhat.com> 2.0.1-4
- Added "Exclusivearch: ix86, x86_64, ia64, ppc, alpha, sparc, sparc64" to
  restrict build to DRI-enabled architectures.

* Thu Jun 08 2006 Mike A. Harris <mharris@redhat.com> 2.0.1-3
- Remove package ownership of mandir/libdir/etc.

* Mon Apr 10 2006 Kristian Høgsberg <krh@redhat.com> 2.0.1-2
- Bump for fc5 build.

* Thu Mar 30 2006 Adam Jackson <ajackson@redhat.com> 2.0.1-1
- Bump to libdrm 2.0.1 from upstream.

* Fri Feb 10 2006 Jesse Keating <jkeating@redhat.com> 2.0-2.2
- bump again for double-long bug on ppc(64)

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> 2.0-2.1
- rebuilt for new gcc4.1 snapshot and glibc changes

* Wed Jan 11 2006 Mike A. Harris <mharris@redhat.com> 2.0-2
- Replaced the temporary tongue-in-cheek humourous package summary and
  description with the proper package descriptions, as many people didn't get
  the joke, while others felt it was getting old.  Ah well, I had my fun for
  a while anyway.  ;o)

* Wed Nov 30 2005 Mike A. Harris <mharris@redhat.com> 2.0-1
- Updated libdrm to version 2.0 from dri.sf.net.  This is an ABI incompatible
  release, meaning everything linked to it needs to be recompiled.

* Tue Nov 01 2005 Mike A. Harris <mharris@redhat.com> 1.0.5-1
- Updated libdrm to version 1.0.5 from dri.sf.net upstream to work around
  mesa unichrome dri driver compile failure.

* Mon Oct 24 2005 Mike A. Harris <mharris@redhat.com> 1.0.4-1
- Updated libdrm to version 1.0.4 from X11R7 RC1
- Remove i915_drv.h, imagine_drv.h, mach64_drv.h, mga_drv.h, mga_ucode.h,
  r128_drv.h, radeon_drv.h, savage_drv.h, sis_drv.h, sis_ds.h, tdfx_drv.h,
  via_drv.h, via_ds.h, via_mm.h, via_verifier.h from file manifest.

* Tue Oct 04 2005 Mike A. Harris <mharris@redhat.com> 1.0.3-3
- Update BuildRoot to use Fedora Packaging Guidelines.
- Add missing "BuildRequires: libX11-devel, pkgconfig"

* Thu Sep 29 2005 Mike A. Harris <mharris@redhat.com> 1.0.3-2
- Add missing documentation to doc macro
- Fix spec file project URL

* Sat Sep 03 2005 Mike A. Harris <mharris@redhat.com> 1.0.3-1
- Initial build.
