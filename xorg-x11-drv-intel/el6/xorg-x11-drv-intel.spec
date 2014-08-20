%define moduledir %(pkg-config xorg-server --variable=moduledir )
%define driverdir	%{moduledir}/drivers
%define gputoolsver 1.7
#define gitdate 20120907
#define gitrev .%{gitdate}

%if 0%{?rhel} == 7
%define rhel7 1
%endif
%if 0%{?rhel} == 6
%define rhel6 1
%endif

%if 0%{?rhel7} || 0%{?fedora} > 17
%define prime 1
%endif

%if 0%{?rhel7} || 0%{?fedora} > 20
%define kmsonly 1
%else
%ifnarch %{ix86}
%define kmsonly 1
%endif
%endif

Summary:   Xorg X11 Intel video driver
Name:      xorg-x11-drv-intel
Version:   2.99.914
Release:   1%{?gitrev}%{?dist}
URL:       http://www.x.org
License:   MIT
Group:     User Interface/X Hardware Support

%if 0%{?gitdate}
Source0:    xf86-video-intel-%{gitdate}.tar.bz2
%else
Source0:    http://xorg.freedesktop.org/archive/individual/driver/xf86-video-intel-%{version}.tar.bz2 
%endif
Source1:    make-intel-gpu-tools-snapshot.sh
Source3:    http://xorg.freedesktop.org/archive/individual/app/intel-gpu-tools-%{gputoolsver}.tar.bz2
Source4:    make-git-snapshot.sh
Patch1:     0001-sna-dri3-Mesa-relies-upon-implicit-fences.patch

ExclusiveArch: %{ix86} x86_64 ia64

BuildRequires: autoconf automake libtool
BuildRequires: flex bison
BuildRequires: xorg-x11-server-devel >= 1.10.99.902
BuildRequires: libXvMC-devel
BuildRequires: mesa-libGL-devel >= 6.5-9
BuildRequires: libdrm-devel >= 2.4.25
#BuildRequires: kernel-headers >= 2.6.32.3
BuildRequires: libudev-devel
BuildRequires: libxcb-devel >= 1.5 
BuildRequires: xcb-util-devel
BuildRequires: cairo-devel
BuildRequires: python

Requires: Xorg %(xserver-sdk-abi-requires ansic)
Requires: Xorg %(xserver-sdk-abi-requires videodrv)
Requires: polkit

%description 
X.Org X11 Intel video driver.

%package devel
Summary:   Xorg X11 Intel video driver development package
Group:     Development/System
Requires:  %{name} = %{version}-%{release}
Provides:  xorg-x11-drv-intel-devel = %{version}-%{release}

%description devel
X.Org X11 Intel video driver development package.

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
%setup -q -n xf86-video-intel-%{?gitdate:%{gitdate}}%{!?gitdate:%{dirsuffix}} -b3
%patch1 -p1

%build
%configure %{?kmsonly:--enable-kms-only} --disable-uxa --enable-sna
make %{?_smp_mflags}

# pushd ../intel-gpu-tools-%{gputoolsver}
# # this is missing from the tarbal, having it empty is ok
# touch lib/check-ndebug.h
# #sed -i 's@PKG_CHECK_MODULES(DRM, \[libdrm_intel >= 2.4.52 libdrm\])@PKG_CHECK_MODULES(DRM, [libdrm_intel >= 2.4.45 libdrm])@' configure.ac
# #sed -i 's@PKG_CHECK_MODULES(CAIRO, \[cairo >= 1.12.0\])@PKG_CHECK_MODULES(CAIRO, [cairo >= 1.8.8])@' configure.ac
# mkdir -p m4
# autoreconf -f -i -v
# # --disable-dumper: quick_dump is both not recommended for packaging yet,
# # and requires python3 to build; i'd like to keep this spec valid for rhel6
# # for at least a bit longer
# %configure %{!?prime:--disable-nouveau} --disable-dumper
# # some of the sources are in utf-8 and pre-preprocessed by python
# export LANG=en_US.UTF-8
# make %{?_smp_mflags}
# popd

%install
%make_install

# pushd ../intel-gpu-tools-%{gputoolsver}
# make install DESTDIR=$RPM_BUILD_ROOT
# rm -f $RPM_BUILD_ROOT%{_bindir}/eudb
# popd

find $RPM_BUILD_ROOT -regex ".*\.la$" | xargs rm -f --

# libXvMC opens the versioned file name, these are useless
rm -f $RPM_BUILD_ROOT%{_libdir}/libI*XvMC.so


%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%doc COPYING
%{driverdir}/intel_drv.so
# %if !%{?kmsonly}
%{_libdir}/libI810XvMC.so.1*
# %endif
%{_libdir}/libIntelXvMC.so.1*
%{_libexecdir}/xf86-video-intel-backlight-helper
%{_datadir}/polkit-1/actions/org.x.xf86-video-intel.backlight-helper.policy
%{_mandir}/man4/i*

%files devel
# %{_bindir}/intel-gen4asm
# %{_bindir}/intel-gen4disasm
# %{_libdir}/pkgconfig/intel-gen4asm.pc

# %files -n intel-gpu-tools
# %doc COPYING
# %{_bindir}/gem_userptr_benchmark
# %{_bindir}/intel*
# %{_mandir}/man1/intel_*.1*

%changelog
* Wed Aug 20 2014 Akemi Yagi <toracat@elrepo.org> - 2.99.914-1.el6.elrepo
- rebuild for ELRepo
- spec file contributed by Rudy Eschauzier <reschauzier@yahoo.com>

* Mon Jul 28 2014 Hans de Goede <hdegoede@redhat.com> - 2.99.914-1
- Rebase to 2.99.914

* Tue Jul 22 2014 Adel Gadllah <adel.gadllah@gmail.com> - 2.99.912-6
- Apply fix for sna render corruption due to missing fencing, FDO #81551

* Fri Jul 11 2014 Hans de Goede <hdegoede@redhat.com> - 2.99.912-5
- Fix a security issue in the backlight helper (CVE-2014-4910)

* Tue Jul  1 2014 Hans de Goede <hdegoede@redhat.com> - 2.99.912-4
- Re-enable DRI3 support (the latest mesa fixes the gnome-shell hang)

* Wed Jun 18 2014 Hans de Goede <hdegoede@redhat.com> - 2.99.912-3
- xserver 1.15.99.903 ABI rebuild

* Thu Jun 12 2014 Hans de Goede <hdegoede@redhat.com> - 2.99.912-2
- DRI3 support causes gnome-shell to hang, disable for now

* Wed Jun 11 2014 Hans de Goede <hdegoede@redhat.com> - 2.99.912-1
- Rebase to 2.99.912
- Rebuild for xserver 1.15.99.903
- Update intel-gpu-tools to 1.7 release

* Sun Jun 08 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.99.911-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Mon Apr 28 2014 Hans de Goede <hdegoede@redhat.com> - 2.99.911-2
- xserver 1.15.99-20140428 git snapshot ABI rebuild
- Add 2 patches from upstream to not close server-fds of udl devices

* Thu Apr 17 2014 Hans de Goede <hdegoede@redhat.com> - 2.99.911-1
- Rebase to 2.99.911
- Rebuild for xserver 1.15.99.902

* Mon Jan 13 2014 Adam Jackson <ajax@redhat.com> - 2.21.15-13
- 1.15 ABI rebuild

* Sat Dec 21 2013 Ville Skyttä <ville.skytta@iki.fi> - 2.21.15-12
- Call ldconfig in %%post* scriptlets.

* Tue Dec 17 2013 Adam Jackson <ajax@redhat.com> - 2.21.15-11
- 1.15RC4 ABI rebuild

* Wed Nov 20 2013 Adam Jackson <ajax@redhat.com> - 2.21.15-10
- 1.15RC2 ABI rebuild

* Wed Nov 06 2013 Adam Jackson <ajax@redhat.com> - 2.21.15-9
- 1.15RC1 ABI rebuild

* Mon Oct 28 2013 Adam Jackson <ajax@redhat.com> - 2.21.15-8
- Don't patch in xwayland in RHEL

* Fri Oct 25 2013 Adam Jackson <ajax@redhat.com> - 2.21.15-7
- ABI rebuild

* Thu Oct 24 2013 Adam Jackson <ajax@redhat.com> 2.21.15-6
- Disable UMS support in F21+

* Thu Oct 24 2013 Adam Jackson <ajax@redhat.com> 2.21.15-5
- xserver 1.15 API compat

* Wed Oct 02 2013 Adam Jackson <ajax@redhat.com> 2.21.15-4
- Default to uxa again

* Mon Sep 23 2013 Adam Jackson <ajax@redhat.com> 2.21.15-2
- Change xwayland requires to be explicitly versioned

* Mon Sep 23 2013 Adam Jackson <ajax@redhat.com> 2.21.15-1
- intel 2.21.15
- xwayland support

* Tue Aug 06 2013 Dave Airlie <airlied@redhat.com> 2.21.14-1
- intel 2.21.24
- add fix to make build - re-enable autoreconf

* Sun Aug 04 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.21.12-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Tue Jul 23 2013 Adam Jackson <ajax@redhat.com> 2.21.12-1
- intel 2.21.12

* Tue Jun 11 2013 Adam Jackson <ajax@redhat.com> 2.21.9-1
- intel 2.21.9
- New i-g-t snapshot
- Drop useless symlinks from -devel
- Repurpose -devel for intel-gen4{,dis}asm
- Default to SNA in F20+

* Tue May 28 2013 Adam Jackson <ajax@redhat.com> 2.21.8-1
- intel 2.21.8

* Fri Apr 12 2013 Dave Airlie <airlied@redhat.com> 2.21.6-1
- intel 2.21.6

* Thu Mar 21 2013 Adam Jackson <ajax@redhat.com> 2.21.5-1
- intel 2.21.5

* Mon Mar 11 2013 Adam Jackson <ajax@redhat.com> 2.21.4-1
- intel 2.21.4

* Thu Mar 07 2013 Adam Jackson <ajax@redhat.com> 2.21.3-1
- intel 2.21.3

* Thu Mar 07 2013 Peter Hutterer <peter.hutterer@redhat.com> - 2.21.2-4
- ABI rebuild

* Fri Feb 15 2013 Peter Hutterer <peter.hutterer@redhat.com> - 2.21.2-3
- ABI rebuild

* Fri Feb 15 2013 Peter Hutterer <peter.hutterer@redhat.com> - 2.21.2-2
- ABI rebuild

* Tue Feb 12 2013 Adam Jackson <ajax@redhat.com> 2.21.2-1
- intel 2.21.2
- New i-g-t snapshot
- Pre-F16 changelog trim

* Wed Jan 16 2013 Adam Jackson <ajax@redhat.com> 2.20.18-2
- Compensate for rawhide's aclocal breaking in a newly stupid way

* Wed Jan 16 2013 Adam Jackson <ajax@redhat.com> 2.20.18-1
- intel 2.20.18

* Tue Jan 08 2013 Dave Airlie <airlied@redhat.com> 2.20.17-2
- Fix damage issue for reverse prime work

* Fri Jan 04 2013 Adam Jackson <ajax@redhat.com> 2.20.17-1
- intel 2.20.17

* Wed Jan 02 2013 Dave Airlie <airlied@redhat.com> 2.20.16-2
- Fix uxa bug that trips up ilk on 3.7 kernels

* Mon Dec 17 2012 Adam Jackson <ajax@redhat.com> 2.20.16-1
- intel 2.20.16

* Wed Nov 28 2012 Adam Jackson <ajax@redhat.com> 2.20.14-1
- intel 2.20.14

* Mon Oct 22 2012 Adam Jackson <ajax@redhat.com> 2.20.12-1
- intel 2.20.12

* Fri Oct 19 2012 Adam Jackson <ajax@redhat.com> 2.20.10-2
- Today's i-g-t
- Don't bother building the nouveau bits of i-g-t on OSes without an X
  server with prime support.

* Mon Oct 15 2012 Dave Airlie <airlied@redhat.com> 2.20.10-1
- intel 2.20.10

* Fri Oct 05 2012 Adam Jackson <ajax@redhat.com> 2.20.9-1
- intel 2.20.9
- Today's intel-gpu-tools snapshot

* Fri Sep 21 2012 Adam Jackson <ajax@redhat.com> 2.20.8-1
- intel 2.20.8

* Mon Sep 10 2012 Adam Jackson <ajax@redhat.com> 2.20.7-1
- intel 2.20.7

* Fri Sep 07 2012 Dave Airlie <airlied@redhat.com> 2.20.6-2
- latest upstream git snapshot with prime + fixes

* Tue Sep 04 2012 Adam Jackson <ajax@redhat.com> 2.20.6-2
- Only bother to build UMS (read: i810) support on 32-bit.  If you've
  managed to build a machine with an i810 GPU but a 64-bit CPU, please
  don't have done that.

* Tue Sep 04 2012 Adam Jackson <ajax@redhat.com> 2.20.6-1
- intel 2.20.6 (#853783)

* Thu Aug 30 2012 Adam Jackson <ajax@redhat.com> 2.20.5-2
- Don't package I810XvMC when not building legacy i810

* Mon Aug 27 2012 Adam Jackson <ajax@redhat.com> 2.20.5-1
- intel 2.20.5

* Mon Aug 20 2012 Adam Jackson <ajax@redhat.com> 2.20.4-3
- Rebuild for new xcb-util soname

* Mon Aug 20 2012 Adam Jackson <ajax@redhat.com> 2.20.4-2
- Backport some patches to avoid binding to non-i915.ko-driven Intel GPUs,
  like Cedarview and friends (#849475)

* Mon Aug 20 2012 Adam Jackson <ajax@redhat.com> 2.20.4-1
- intel 2.20.4

* Thu Aug 16 2012 Dave Airlie <airlied@redhat.com> 2.20.3-3
- fix vmap flush to correct upstream version in prime patch

* Thu Aug 16 2012 Dave Airlie <airlied@redhat.com> 2.20.3-2
- snapshot upstream + add prime support for now

* Wed Aug 15 2012 Adam Jackson <ajax@redhat.com> 2.20.3-1
- intel 2.20.3

* Wed Aug 01 2012 Adam Jackson <ajax@redhat.com> 2.20.2-1
- intel 2.20.2
- Only disable UMS in RHEL7, since i810 exists in RHEL6

* Mon Jul 23 2012 Adam Jackson <ajax@redhat.com> 2.20.1-1
- intel 2.20.1

* Sun Jul 22 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.20.0-2.20120718
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Wed Jul 18 2012 Dave Airlie <airlied@redhat.com> 2.20.0-1.20120718
- todays git snapshot

* Tue Jun 12 2012 Dave Airlie <airlied@redhat.com> 2.19.0-5.20120612
- today's git snapshot
- resurrect copy-fb

* Tue May 29 2012 Adam Jackson <ajax@redhat.com> 2.19.0-4.20120529
- Today's git snapshot
- Enable SNA (default is still UXA, use Option "AccelMethod" to switch)
- build-fix.patch: Fix build with Fedora's default cflags

* Tue May 29 2012 Adam Jackson <ajax@redhat.com> 2.19.0-3
- Don't autoreconf the driver, fixes build on F16.

* Mon May 21 2012 Adam Jackson <ajax@redhat.com> 2.19.0-2
- Disable UMS support in RHEL.
- Trim some Requires that haven't been needed since F15.

* Thu May 03 2012 Adam Jackson <ajax@redhat.com> 2.19.0-1
- intel 2.19.0
