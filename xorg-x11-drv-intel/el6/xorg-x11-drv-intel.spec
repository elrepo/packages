%define legacyname  xorg-x11-drv-i810
%define legacyver   2.6.0-8
%define moduledir %(pkg-config xorg-server --variable=moduledir )
%define driverdir	%{moduledir}/drivers
%define gputoolsdate 20100416
#define gitdate 20100326

Summary:   Xorg X11 Intel video driver
Name:      xorg-x11-drv-intel
Version:   2.17.0
Release:   2%{?dist}
URL:       http://www.x.org
License:   MIT
Group:     User Interface/X Hardware Support
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Source0:    http://xorg.freedesktop.org/archive/individual/driver/xf86-video-intel-%{version}.tar.bz2 
#Source0:    xf86-video-intel-%{gitdate}.tar.bz2
Source1:    make-intel-gpu-tools-snapshot.sh
Source2:    intel.xinf
Source3:    intel-gpu-tools-%{gputoolsdate}.tar.bz2
Source4:    make-git-snapshot.sh

#Patch1: kill-svideo.patch
#Patch2: copy-fb.patch

# needs to be upstreamed
#Patch20: intel-2.8.0-kms-get-crtc.patch
#Patch21: intel-2.11-lvds-first.patch
#Patch22: intel-2.11.0-vga-clock-max.patch

#Patch60: uevent.patch

# https://bugs.freedesktop.org/show_bug.cgi?id=27885
#Patch61: intel-2.10.0-add-mbp-backlight.patch

# https://bugzilla.redhat.com/588421
#Patch62: intel-2.11-no-pageflipping.patch

# https://bugzilla.redhat.com/604024
#Patch63: intel-2.11.0-fix-rotate-flushing-965.patch

#Patch64: intel-no-sandybridge.patch

ExclusiveArch: %{ix86} x86_64 ia64

BuildRequires: autoconf automake libtool
BuildRequires: xorg-x11-server-devel >= 1.4.99.1
BuildRequires: libXvMC-devel
BuildRequires: mesa-libGL-devel >= 6.5-9
BuildRequires: libdrm-devel >= 2.4.31
BuildRequires: kernel-headers
BuildRequires: libudev-devel
BuildRequires: libxcb-devel >= 1.5 
BuildRequires: xcb-util-devel
BuildRequires: libdmx-devel
BuildRequires: libXtst-devel
BuildRequires: libXxf86dga-devel
BuildRequires: libXxf86misc-devel
BuildRequires: xorg-x11-util-macros >= 1.15.0
BuildRequires: xorg-x11-proto-devel >= 7.6-17

Requires:  hwdata
Requires:  xorg-x11-server-Xorg >= 1.4.99.1
Requires:  libxcb >= 1.5
Requires:  xcb-util
Requires:  libdrm >= 2.4.31

Requires:  kernel >= 2.6.32-33.el6
Provides:   %{legacyname} = %{legacyver}
Obsoletes:  %{legacyname} < %{legacyver}

%description 
X.Org X11 Intel video driver.

%package devel
Summary:   Xorg X11 Intel video driver XvMC development package
Group:     Development/System
Requires:  %{name} = %{version}-%{release}
Provides:  xorg-x11-drv-intel-devel = %{version}-%{release}
Provides:   %{legacyname}-devel = %{legacyver}
Obsoletes:  %{legacyname}-devel < %{legacyver}

%description devel
X.Org X11 Intel video driver XvMC development package.

%package -n intel-gpu-tools
Summary:    Debugging tools for Intel graphics chips
Group:	    Development/Tools

%description -n intel-gpu-tools
Debugging tools for Intel graphics chips

%if 0%{?gitdate}
%define dirsuffix %{gitdate}
%else
%define dirsuffix %{version}
%endif

%prep
%setup -q -n xf86-video-intel-%{dirsuffix} -b3
#%patch1 -p1 -b .svideo
#%patch2 -p1 -b .copy-fb
#%patch20 -p1 -b .get-crtc
#%patch21 -p1 -b .lvds-first
#%patch22 -p1 -b .vga-clock
#%patch60 -p1 -b .uevent
#%patch61 -p1 -b .mbp-backlight
#%patch62 -p1 -b .no-flip
#%patch63 -p1 -b .rotateflush
#%patch64 -p1 -b .snb

%build
 
# Need autoreconf also when patching a release (to pick up -ludev)
autoreconf -vi

%configure --disable-static --libdir=%{_libdir} --mandir=%{_mandir} --enable-dri --enable-xvmc
make

pushd ../intel-gpu-tools-%{gputoolsdate}
autoreconf -v --install
%configure
make
popd

%install
rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT%{_datadir}/hwdata/videoaliases
install -m 0644 %{SOURCE2} $RPM_BUILD_ROOT%{_datadir}/hwdata/videoaliases/

pushd ../intel-gpu-tools-%{gputoolsdate}
make install DESTDIR=$RPM_BUILD_ROOT
popd

find $RPM_BUILD_ROOT -regex ".*\.la$" | xargs rm -f --

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{driverdir}/intel_drv.so
%{_datadir}/hwdata/videoaliases/intel.xinf
%{_libdir}/libI810XvMC.so.1*
%{_libdir}/libIntelXvMC.so.1*
%{_mandir}/man4/i*

%files devel
%defattr(-,root,root,-)
%{_libdir}/libI810XvMC.so
%{_libdir}/libIntelXvMC.so

%files -n intel-gpu-tools
%defattr(-,root,root,-)
%{_bindir}/intel_*
%{_mandir}/man1/intel_*.1*

%changelog
* Sun Feb 26 2012 Phil Schaffner <pschaff2@verison.net> 2.17.0-2
- build with libdrm 2.4.31

* Sun Jan 01 2012 Phil Schaffner <p.r.schaffner@ieee.org> 2.17.0-1
- 2.17.0

* Fri Sep 16 2011 Phil Schaffner <p.r.schaffner@ieee.org> 2.16.0-1
- 2.16.0

* Sat May 14 2011 Phil Schaffner <p.r.schaffner@ieee.org> 2.15.0-1
- 2.15.0

* Fri Aug 13 2010 Adam Jackson <ajax@redhat.com> 2.11.0-7
- intel-no-sandybridge.patch: Don't try to bind to snb devices (#624132)

* Fri Jun 25 2010 Dave Airlie <airlied@redhat.com> 2.11.0-6
- intel-2.11.0-fix-rotate-flushing-965.patch: fix rotated lags (#604024)

* Fri Jun 04 2010 Dave Airlie <airlied@redhat.com> 2.11.0-5
- fix X -nr (requires kernel 2.6.32-33 to work).

* Mon May 03 2010 Adam Jackson <ajax@redhat.com> 2.11.0-4
- intel-2.11-no-pageflipping.patch: Disable pageflipping (#588421)

* Fri Apr 30 2010 Bastien Nocera <bnocera@redhat.com> 2.11.0-3
- Add MacBook backlight support

* Mon Apr 26 2010 Adam Jackson <ajax@redhat.com> 2.11.0-2
- intel-2.11.0-vga-clock-max.patch: Clamp VGA pixel clock to 250MHz,
  anything higher's going to look awful anyway. (#559426)

* Fri Apr 16 2010 Adam Jackson <ajax@redhat.com> 2.11.0-1
- intel 2.11.0
- new gpu tools snapshot

* Fri Mar 26 2010 Adam Jackson <ajax@redhat.com> 2.10.0-5
- New driver snapshot (2.10.92ish).
- New GPU tools snapshot.

* Wed Feb 10 2010 Adam Jackson <ajax@redhat.com> 2.10.0-4
- Remove call to I830EmitFlush (#563212)

* Mon Feb 08 2010 Adam Jackson <ajax@redhat.com> 2.10.0-3
- Re-apply patches.

* Thu Jan 21 2010 Peter Hutterer <peter.hutterer@redhat.com> - 2.10.0-2
- Rebuild for server 1.8

* Wed Jan 13 2010 Dave Airlie <airlied@redhat.com> 2.10.0-1
- intel 2.10.0 - needs libxcb for XvMC client

* Mon Oct 26 2009 Adam Jackson <ajax@redhat.com> 2.9.1-1
- intel 2.9.1

* Fri Oct 09 2009 Dave Airlie <airlied@redhat.com> 2.9.0-3
- set gamma on mode set major for kms

* Thu Oct  1 2009 Kristian Høgsberg <krh@hinata> - 2.9.0-2
- Rebase to 2.9.0.
- Need autoreconf also when patching a release (to pick up -ludev)

* Thu Sep 24 2009 Dave Airlie <airlied@redhat.com> 2.8.0-16.20090909
- Attempt to make -nr work again

* Fri Sep 18 2009 Adam Jackson <ajax@redhat.com> 2.8.0-15.20090909
- lvds-modes.patch: Fix to work in more cases.

* Thu Sep 17 2009 Kristian Høgsberg <krh@redhat.com> - 2.8.0-14.20090909
- Drop page flip patch.

* Wed Sep 09 2009 Adam Jackson <ajax@redhat.com> 2.8.0-13.20090909
- Today's git snap

* Tue Sep 08 2009 Adam Jackson <ajax@redhat.com> 2.8.0-12.20090908
- lvds-modes.patch: Remove egregiously silly LVDS mode list construction,
  use an algorithm (ported from radeon) that actually works instead.

* Tue Sep 08 2009 Adam Jackson <ajax@redhat.com> 2.8.0-11.20090908
- Today's git snap (driver and tools)

* Tue Sep 08 2009 Adam Jackson <ajax@redhat.com> 2.8.0-10
- kill-svideo.patch: Treat svideo as unknown instead of disconnected.

* Wed Sep 02 2009 Adam Jackson <ajax@redhat.com> 2.8.0-9
- uevent.patch: Catch uevents for output hotplug, rescan when they happen,
  and send RANDR events for any changes.

* Thu Aug 20 2009 Kristian Høgsberg <krh@redhat.com> - 2.8.0-8
- Don't use fb offset when using shadow fbs.

* Wed Aug 12 2009 Adam Jackson <ajax@redhat.com> 2.8.0-6
- Today's driver snapshot, misc bugfixes.

* Mon Aug 10 2009 Adam Jackson <ajax@redhat.com> 2.8.0-5
- intel-2.8.0-lvds-first.patch: Put LVDS outputs first in the list in KMS to
  match old UMS behaviour.

* Fri Aug 7 2009 Kristian Høgsberg <krh@redhat.com> - 2.8.0-4
- Add dri2-page-flip.patch to enable full screen pageflipping.
  Fixes XKCD #619.

* Tue Aug 04 2009 Dave Airlie <airlied@redhat.com> 2.8.0-3
- add ABI fixes for RAC removal

* Mon Jul 27 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.8.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Tue Jul 21 2009 Adam Jackson <ajax@redhat.com> 2.8.0-1
- intel 2.8.0
- Fix chip names to match pci.ids where no official product name is known.

* Wed Jul 15 2009 Matthias Clasen <mclasen@redhat.com> - 2.8.0-0.3
- Rebuild for new API

* Tue Jul 14 2009 Adam Jackson <ajax@redhat.com> 2.8.0-0.2
- Today's git snapshots (driver and gpu tools)
- intel-2.7-dont-vsync-xv.patch: Drop, should be working now.

* Wed Jun 24 2009 Adam Jackson <ajax@redhat.com> 2.8.0-0.1
- Today's git snapshots (driver and gpu tools)

* Mon Jun 22 2009 Adam Jackson <ajax@redhat.com> 2.7.0-8
- Fix ABI for new server version

* Thu May 28 2009  <krh@redhat.com> - 2.7.0-7
- Add intel-2.7-dont-vsync-xv.patch to disable Xv vsync, fixes hw
  lockup for full screen Xv (#499895)

* Wed May 20 2009  <krh@madara.bos.redhat.com> - 2.7.0-6
- Add intel-2.7-dont-gtt-map-big-objects.patch to avoid mapping big
  gem objects through the GTT (#498131).

* Thu May 07 2009 Adam Jackson <ajax@redhat.com> 2.7.0-5
- Update intel-gpu-tools

* Wed May  6 2009  <krh@madara.bos.redhat.com> - 2.7.0-4
- Pull in fixes from the 2.7 branch.

* Thu Apr 23 2009 Adam Jackson <ajax@redhat.com> 2.7.0-2
- Add intel-gpu-tools subpackage

* Fri Apr 17 2009 Adam Jackson <ajax@redhat.com> 2.7.0-1
- intel 2.7.0

* Mon Apr 13 2009 Adam Jackson <ajax@redhat.com> 2.6.99.902-3
- Update to today's git snapshot.

* Mon Apr  6 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.99.902-2
- Update to newer 2.7 snapshot, drop no-legacy3d.patch.

* Thu Mar 12 2009 Adam Jackson <ajax@redhat.com> 2.6.99.902-1
- intel-2.6.99.902-kms-get-crtc.patch: Add drmmode_get_crtc() so we
  blink less on VT switches.

* Wed Mar 11 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.99.902-0
- Use 2.7 rc2 tarball.
- Consolidate the legacy3d patches into no-legacy3d.patch, which sent
  upstream.

* Thu Mar  5 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-14
- Fix copy-fb patch to skip dpms off on initial modeset.
- Downgrade the conflicts to a requires.

* Thu Mar  5 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-13
- Add conflicts to make sure we have a new enough kernel that
  drmSetMaster() doesn't deadlock.

* Wed Mar  4 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-12
- Update to new git snapshot: re-enables multiple X servers, fixes
  leak on vt switch for 965 type hardware, fold dpms patch into git
  master patch.

* Mon Mar 02 2009 Adam Jackson <ajax@redhat.com> 2.6.0-11
- intel-2.6.0-kms-dpms.patch: Enable DPMS on KMS outputs.

* Sat Feb 28 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-10
- Update to master again, should fix the 1MB per pixmap problem.
- Drop no-op intel-2.1.1-fix-xv-reset.patch.

* Tue Feb 24 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-9
- Fix copy-fb to be less dumb about picking the crtc.

* Tue Feb 24 2009 Adam Jackson <ajax@redhat.com> 2.6.0-8
- Rename to xorg-x11-drv-intel

* Tue Feb 24 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-7
- Update to git master, pull in patches to kill svideo and copy fb contents.

* Wed Feb 18 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-6
- Update to git master again, fixes Xv and xgamma.

* Fri Feb 13 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-5
- Add patch.

* Fri Feb 13 2009 Kristian Høgsberg <krh@redhat.com> - 2.6.0-4
- Update snapshot to pull in KMS framebuffer resize.

* Sun Feb 08 2009 Adam Jackson <ajax@redhat.com> 2.6.0-3
- Bump libdrm BR. (#480299)

* Thu Jan 29 2009 Kristian Høgsberg <krh@hinata> - 2.6.0-2
- Update to 66bc44 from git master.

* Tue Jan 20 2009 Kristian Høgsberg <krh@kabuto.bos.redhat.com> 2.6.0-1
- Update to 2.6.0.

* Mon Dec 29 2008 Dave Airlie <airlied@redhat.com> 2.5.99.1-0.3
- enable KMS code in driver

* Mon Dec 22 2008 Dave Airlie <airlied@redhat.com> 2.5.99.1-0.2
- rebuild for new server API

* Sun Dec 21 2008 Dave Airlie <airlied@redhat.com> 2.5.99.1-0.1
- intel rebase to upstream release + master fixes

* Mon Nov 10 2008 Adam Jackson <ajax@redhat.com> 2.5.0-3
- intel-2.4.2-dell-quirk.patch: No LVDS on Dell Studio Hybrid.

* Fri Oct 31 2008 Dave Airlie <airlied@redhat.com> 2.5.0-2
- disable legacy 3D allocation now we have GEM.

* Tue Oct 21 2008 Dave Airlie <airlied@redhat.com> 2.5.0-1
- rebase to Intel 2.5.0 release

* Tue Oct 14 2008 Dave Airlie <airlied@redhat.com> 2.4.2-12
- intel-2.4.2-cantiga-fix.patch - hopefully fix cantiga

* Tue Oct 14 2008 Adam Jackson <ajax@redhat.com> 2.4.2-11
- intel-2.4.2-macmini-fix.patch: Fix a segfault on Mac Mini.

* Tue Oct 14 2008 Dave Airlie <airlied@redhat.com> 2.4.2-10
- rebase to latest upstream master

* Wed Oct 01 2008 Dave Airlie <airlied@redhat.com> 2.4.2-9
- rebase to upstream for new libdrm interfaces

* Thu Sep 11 2008 Soren Sandmann <sandmann@redhat.com> 2.4.2-8
- Remove the fb size hack, since there is a fix in the server now.

* Wed Sep 10 2008 Adam Jackson <ajax@redhat.com> 2.4.2-7
- Do the fb size hack a different terrible way.

* Tue Sep 09 2008 Dave Airlie <airlied@redhat.com> 2.4.2-6
- fix typo in drmmode display.c

* Mon Sep 08 2008 Adam Jackson <ajax@redhat.com> 2.4.2-5
- intel-2.4.2-fb-size.patch: Yet more lame heuristics to preallocate a
  usable framebuffer for laptops. (#458864)

* Mon Sep 08 2008 Dave Airlie <airlied@redhat.com> 2.4.2-4
- Add patch from fd.o bug 17341 to fix problems on intel EXA

* Wed Sep 03 2008 Dave Airlie <airlied@redhat.com> 2.4.2-3
- intel-fix-irq.patch - Don't die on irq handler failure
- I think krh DRI2 patches broke it.

* Thu Aug 28 2008 Dave Airlie <airlied@redhat.com> 2.4.2-2
- upgrade to git head - brings in modesetting + GEM bits - fix flip

* Tue Aug 26 2008 Adam Jackson <ajax@redhat.com> 2.4.2-1
- intel 2.4.2.

* Tue Aug 12 2008 Adam Jackson <ajax@redhat.com> 2.4.0-2
- Fix module loading.  D'oh.

* Mon Aug 11 2008 Adam Jackson <ajax@redhat.com> 2.4.0-1
- intel 2.4.0.
- Switch back to EXA by default.  Let's see if it works this time.

* Tue Apr  8 2008 Bill Nottingham <notting@redhat.com> - 2.2.1-20
- disable framebuffer compression by default (fdo #13326)

* Wed Apr  2 2008 Kristian Høgsberg <krh@redhat.com> - 2.2.1-19
- Rebase batchbuffer driver to pull in fix for EAGAIN handling around
  batchbuffer submit ioctl.

* Wed Apr  2 2008 Kristian Høgsberg <krh@redhat.com> - 2.2.1-18
- Tweak intel-stub.c and batchbuffer branch to read options from
  server flags section too.

* Tue Apr  1 2008 Kristian Høgsberg <krh@redhat.com> - 2.2.1-17
- Add new snapshot of the batchbuffer driver to go with the DRI2 changes.
- Add "DRI2" as a server layout options to enable batchbuffer and DRI2.

* Tue Apr 01 2008 Adam Jackson <ajax@redhat.com> 2.2.1-16
- intel-stub.c: Remember the i810 users! (#439845)

* Tue Mar 25 2008 Jeremy Katz <katzj@redhat.com> - 2.2.1-15
- Add jbarnes's backlight test patch

* Tue Mar 18 2008 Dave Airlie <airlied@redhat.com> 2.2.1-14
- make XAA default for now so installer can be run on 965 hw

* Tue Mar 18 2008 Dave Airlie <airlied@redhat.com> 2.2.1-13
- fix modesetting for normal DRI codepath

* Fri Mar 14 2008 Dave Airlie <airlied@redhat.com> 2.2.1-12
- fix modesetting vt switch to not hit the non-existant ring

* Wed Mar 12 2008 Dave Airlie <airlied@redhat.com> 2.2.1-11
- fall intel master driver back to non-TTM mode avoids compiz fail

* Mon Mar 10 2008 Dave Airlie <airlied@redhat.com> 2.2.1-10
- quirk Motion Computing M1200

* Fri Mar 07 2008 Dave Airlie <airlied@redhat.com> 2.2.1-9
- update modesetting patch to include 965 video + fix for memory
  space leak

* Fri Mar 07 2008 Dave Airlie <airlied@redhat.com> 2.2.1-8
- fixup pciaccess version check and autoconf and fallout

* Fri Mar 07 2008 Dave Airlie <airlied@redhat.com> 2.2.1-7
- re-run autoconf to build modesetting with batchbuffer

* Thu Mar 06 2008 Dave Airlie <airlied@redhat.com> 2.2.1-6
- fix modesetting to start on i965 chips

* Thu Mar 06 2008 Dave Airlie <airlied@redhat.com> 2.2.1-5
- Bump to include modesetting driver - and make stub auto pick
  batchbuffer branch if modesetting enabled

* Mon Mar  3 2008 Kristian Høgsberg <krh@redhat.com> - 2.2.1-4
- Bump intel-batchbuffer to latest snapshot, rebuild against new server ABI.

* Mon Mar 03 2008 Dave Airlie <airlied@redhat.com> 2.2.1-3
- update for new server abi

* Thu Feb 28 2008 Adam Jackson <ajax@redhat.com> 2.2.1-2
- intel-2.1.1-efi.patch: Fix SDVO I2C on Mac Mini in EFI mode.

* Wed Feb 27 2008 Kristian Høgsberg <krh@redhat.com> - 2.2.1-1
- Bump to 2.2.1, include build of the intel-batchbuffer branch.

* Wed Feb 20 2008 Fedora Release Engineering <rel-eng@fedoraproject.org> - 2.2.0-4
- Autorebuild for GCC 4.3

* Wed Jan 09 2008 Adam Jackson <ajax@redhat.com> 2.2.0-3
- Rebuild for new server ABI.
- intel-2.2.0-alloca.patch: Fix use of {DE,}ALLOCATE_LOCAL.

* Mon Dec 10 2007 Dave Airlie <airlied@redhat.com> 2.2.0-2
- hook up ch7017 (bz#408471)

* Tue Nov 27 2007 Adam Jackson <ajax@redhat.com> 2.2.0-1
- xf86-video-intel 2.2.0

* Tue Nov 13 2007 Adam Jackson <ajax@redhat.com> 2.1.99-1
- xf86-video-intel 2.1.99.
- Drop the i810 driver.  Time to move on.
- Require xserver 1.4.99.1

* Wed Oct 17 2007 Dave Airlie <airlied@redhat.com> 2.1.1-7
- intel-2.1.1-fix-xv-compiz.patch - update to not crash is we can't get RAM

* Wed Oct 17 2007 Dave Airlie <airlied@redhat.com> 2.1.1-6
- intel-2.1.1-fix-xv-compiz.patch - Real dirty hack to allocate 4MB of RAM
  for textured Xv to use as an offscreen pixmap so xv actually works under
  compiz - granted it may be unusably slow but at least stuff shows up.

* Mon Oct 15 2007 Dave Airlie <airlied@redhat.com> 2.1.1-5
- intel-2.1.1-fix-vt-switch.patch - Only restore paletter regs on enabled pipes
- intel-2.1.1-fix-xv-reset.patch - Reset XV after mode switch

* Fri Oct 05 2007 Dave Airlie <airlied@redhat.com> 2.1.1-4
- intel-2.1.1-quirk-update.patch - update quirks from master

* Mon Aug 20 2007 Adam Jackson <ajax@redhat.com> 2.1.1-3
- i810.xinf: Flip everything over to -intel by default now.  Still install
  i810 driver just in case.

* Wed Aug 15 2007 Dave Airlie <airlied@redhat.com> 2.1.1-2
- intel-2.1.1-fix-texoffset-start.patch - shouldn't set texoffsetstart when not using EXA

* Tue Aug 14 2007 Dave Airlie <airlied@redhat.com> 2.1.1-1
- xf86-video-intel 2.1.1.

* Tue Jul 03 2007 Adam Jackson <ajax@redhat.com> 2.1.0-1
- xf86-video-intel 2.1.0.

* Mon Jun 18 2007 Adam Jackson <ajax@redhat.com> 2.0.0-5
- Update Requires and BuildRequires.

* Wed Jun 06 2007 Adam Jackson <ajax@redhat.com> 2.0.0-4
- Update to git master.  Many Xv and DVO fixes.  Adds support for 945GME,
  965GME, G33, Q33, and Q35 chips.

* Mon May 14 2007 Adam Jackson <ajax@redhat.com> 2.0.0-3
- intel-2.0-vblank-power-savings.patch: Disable vblank interrupts when no
  DRI clients are active, for better battery life.

* Tue May 01 2007 Adam Jackson <ajax@redhat.com> 2.0.0-2
- Rebuild for final RANDR 1.2 ABI.  Fixes segfault at startup. (#238575)

* Mon Apr 23 2007 Adam Jackson <ajax@redhat.com> 2.0.0-1
- xf86-video-intel 2.0.0.  Change the version number to match, why not.
- Add a Virtual provides for xorg-x11-drv-intel, since we should probably
  rename this at some point.

* Tue Apr 10 2007 Adam Jackson <ajax@redhat.com> 1.6.5-19
- i810.xinf: Move all 965 and 945 chips onto the new driver, as well as
  915GM.

* Thu Apr 05 2007 Adam Jackson <ajax@redhat.com> 1.6.5-18
- i810.xinf: More intel whitelisting (#214011, #234877)

* Wed Apr 04 2007 Adam Jackson <ajax@redhat.com> 1.6.5-17
- xf86-video-intel-1.9.94 (RC4).  Adds support for 965GM.
- i810.xinf: Point 965GM support at the intel driver since it's not present
  in old i810.

* Fri Mar 30 2007 Adam Jackson <ajax@redhat.com> 1.6.5-16
- xf86-video-intel-1.9.93 (RC3).

* Tue Mar 27 2007 Jeremy Katz <katzj@redhat.com> - 1.6.5-15
- fix typo with 945GM pci id from my laptop

* Thu Mar 22 2007 Adam Jackson <ajax@redhat.com> 1.6.5-14
- xf86-video-intel 1.9.92 (RC2).

* Mon Mar 05 2007 Adam Jackson <ajax@redhat.com> 1.6.5-13
- Updated modesetting driver to one that will actually work with a 1.3 server.

* Tue Feb 27 2007 Adam Jackson <ajax@redhat.com> 1.6.5-12
- Nuke %%with_dri, since the arch list exactly matched the ExclusiveArch list
- Remove ivch and ch7017 from the install since they aren't hooked up to the
  code anywhere
- Disown the module

* Tue Jan 30 2007 Jeremy Katz <katzj@redhat.com> - 1.6.5-11
- update modesetting driver to git snapshot from today

* Tue Nov 7 2006 Adam Jackson <ajackson@redhat.com> 1.6.5-10
- i965-xv-hang-fix.patch: Backport Xv hang fix for G965.

* Sun Oct 01 2006 Jesse Keating <jkeating@redhat.com> - 1.6.5-9
- rebuilt for unwind info generation, broken in gcc-4.1.1-21

* Fri Sep 22 2006 Adam Jackson <ajackson@redhat.com> 1.6.5-8.fc6
- Change 'Requires: kudzu >= foo' to 'Conflicts: kudzu < foo' since we don't
  actually require kudzu to run.

* Fri Sep 15 2006 Adam Jackson <ajackson@redhat.com> 1.6.5-7.fc6
- i810.xinf: Whitelist Apple 945GM machines and Aopen Mini PC onto intel(4)

* Tue Sep 12 2006 Adam Jackson <ajackson@redhat.com> 1.6.5-6.fc6
- i810-1.6.5-to-git-20060911.patch: Backport post-1.6.5 fixes from git.
- i810-match-server-sync-ranges.patch: Make a terrible heuristic in the
  driver match the corresponding terrible heuristic in the server.

* Mon Aug 28 2006 Adam Jackson <ajackson@redhat.com> 1.6.5-5.fc6
- intel-945gm-lfp-blacklist.patch: Tweak the Apple blacklist to (hopefully)
  correctly distinguish between Mac Mini and Macbook Pro.

* Mon Aug 21 2006 Adam Jackson <ajackson@redhat.com> 1.6.5-4.fc6
- i810.xinf: PCI IDs for i965.

* Thu Aug 17 2006 Adam Jackson <ajackson@redhat.com> 1.6.5-3.fc6
- i810.xinf: Uppercase PCI IDs.

* Fri Aug 10 2006 Adam Jackson <ajackson@redhat.com> 1.6.5-2.fc6
- Update i810 to 1.6.5, should fix DRI.
- Add kuzdu requires.
- i810.xinf: Start whitelisting devices over to intel.

* Wed Aug  9 2006 Adam Jackson <ajackson@redhat.com> 1.6.4-3.fc6
- intel-driver-rename.patch: Fix the driver name in more places so it'll,
  you know, load.

* Wed Aug  9 2006 Adam Jackson <ajackson@redhat.com> 1.6.4-2.fc6
- intel-945gm-lfp-blacklist.patch: At anholt's suggestion, remove the other
  LFP special casing in favor of the blacklist.

* Wed Aug  9 2006 Adam Jackson <ajackson@redhat.com> 1.6.4-1.fc6
- Admit defeat, kinda.  Package both i810 stable and modesetting drivers.
  The modesetting driver is installed as intel_drv.so instead of i810_drv.so,
  and is selected with Driver "intel" in xorg.conf.  Individual devices will
  whitelist over to "intel" until that branch gets merged into head.
- Update the stable branch driver to 1.6.4 from upstream, adds i965 support.
- intel-945gm-lfp-blacklist.patch: Blacklist LFP detection on machines where
  the BIOS is known to lie.

* Tue Aug  8 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-14.20060808modeset.fc6
- Today's snapshot: I2C bus creation fix.

* Wed Aug  2 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-13.20060717modeset.fc6
- intel-prune-by-edid-pixclock.patch: Honor the EDID-reported maximum pixel
  clock when computing the modes list.
- intel-virtual-sizing-bogon.patch: Don't interpret the size of the display
  in centimeters as the size of the display in pixels.

* Mon Jul 24 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-12.20060717modeset.fc6
- Disable spread-spectrum LVDS, various crash and hang fixes, saner output
  probing.

* Thu Jul 13 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-11.20060713modeset.fc6
- Update again for a mode comparison bugfix.

* Thu Jul 13 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-10.20060713modeset.fc6
- Update to today's git; crash fixes, better pre-915 support, slightly better
  autoconfigurability.

* Wed Jul 12 2006 Jesse Keating <jkeating@redhat.com> 1.6.0-9.20060707modeset.1.fc6
- rebuild

* Tue Jul 11 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-9.20060707modeset
- Fix Revision number to match naming policy.

* Tue Jul 11 2006 Kristian Høgsberg <krh@redhat.com> 1.6.0-8.modeset20060707
- Add back modesetting changes.

* Mon Jul 10 2006 Kristian Høgsberg <krh@redhat.com> 1.6.0-7
- Roll back modesetting changes and build for fc5 aiglx repo.

* Fri Jul  7 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-6.modeset20060707
- Snapshot of the git modesetting branch.

* Fri Jul  7 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-6
- Update i810.xinf to include entries for E7221 and 945GM.

* Fri Jun 23 2006 Mike A. Harris <mharris@redhat.com> 1.6.0-5
- Add with_dri macro to spec file, and conditionalize build time DRI support

* Fri May 26 2006 Mike A. Harris <mharris@redhat.com> 1.6.0-4
- Added "BuildRequires: libdrm >= 2.0-1" for (#192334), and updated sdk dep
  to pick up proto-devel as well.

* Tue May 23 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-3
- Rebuild for 7.1 ABI fix.

* Tue Apr 11 2006 Kristian Høgsberg <krh@redhat.com> 1.6.0-2
- Bump for fc5-bling build.

* Sun Apr 09 2006 Adam Jackson <ajackson@redhat.com> 1.6.0-1
- Update to 1.6.0 from 7.1RC1.

* Tue Apr 04 2006 Kristian Høgsberg <krh@redhat.com> 1.4.1.3-4.cvs20060322.1
- Add patch to add missing #include's, specifically assert.h.

* Wed Mar 22 2006 Kristian Høgsberg <krh@redhat.com> 1.4.1.3-4.cvs20060322
- Update to CVS snapshot of 20060322.

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> 1.4.1.3-3.1
- rebuilt for new gcc4.1 snapshot and glibc changes

* Sat Feb 04 2006 Mike A. Harris <mharris@redhat.com> 1.4.1.3-3
- Added 8086:2772 mapping to i810.xinf for bug (#178451)

* Fri Feb 03 2006 Mike A. Harris <mharris@redhat.com> 1.4.1.3-2
- Added 8086:2592 mapping to i810.xinf for bug (#172884)

* Wed Jan 18 2006 Mike A. Harris <mharris@redhat.com> 1.4.1.3-1
- Updated xorg-x11-drv-i810 to version 1.4.1.3 from X11R7.0

* Tue Dec 20 2005 Mike A. Harris <mharris@redhat.com> 1.4.1.2-1
- Updated xorg-x11-drv-i810 to version 1.4.1.2 from X11R7 RC4
- Removed 'x' suffix from manpage dirs to match RC4 upstream.

* Wed Nov 16 2005 Mike A. Harris <mharris@redhat.com> 1.4.1-1
- Updated xorg-x11-drv-i810 to version 1.4.1 from X11R7 RC2

* Fri Nov 04 2005 Mike A. Harris <mharris@redhat.com> 1.4.0.1-1
- Updated xorg-x11-drv-i810 to version 1.4.0.1 from X11R7 RC1
- Fix *.la file removal.
- Added 'devel' subpackage for XvMC .so
- Added 'BuildRequires: libXvMC-devel' for XvMC drivers.

* Mon Oct 03 2005 Mike A. Harris <mharris@redhat.com> 1.4.0-1
- Update BuildRoot to use Fedora Packaging Guidelines.
- Deglob file manifest.
- Limit "ExclusiveArch" to x86, x86_64, ia64

* Fri Sep 02 2005 Mike A. Harris <mharris@redhat.com> 1.4.0-0
- Initial spec file for i810 video driver generated automatically
  by my xorg-driverspecgen script.
