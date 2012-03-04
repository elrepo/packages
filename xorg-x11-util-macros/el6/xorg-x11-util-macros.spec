%define pkgname util-macros
%define debug_package %{nil}

Summary: X.Org X11 Autotools macros
Name: xorg-x11-util-macros
Version: 1.15.0
Release: 1%{?dist}
License: MIT
Group: Development/System
URL: http://www.x.org
BuildArch: noarch
Source0:  ftp://ftp.x.org/pub/individual/util/util-macros-%{version}.tar.bz2
Requires: autoconf automake libtool pkgconfig

%description
X.Org X11 autotools macros required for building the various packages that
comprise the X Window System.

%prep
%setup -q -n %{pkgname}-%{version}

%build
%configure
make %{?_smp_mflags}

%install
make install DESTDIR=$RPM_BUILD_ROOT INSTALL="install -p"
rm -f $RPM_BUILD_ROOT%{_datadir}/util-macros/INSTALL

%files
%defattr(-,root,root,-)
%doc COPYING ChangeLog
%{_datadir}/aclocal/xorg-macros.m4
%{_datadir}/pkgconfig/xorg-macros.pc

%changelog
* Sun Jan 01 2012 Phil Schaffner <p.r.schaffner@ieee.org> 1.15.0-1
- Initial ELRepo build

* Tue Jul 05 2011 Adam Jackson <ajax@redhat.com> 1.15.0-1
- util-macros 1.15

* Mon May 30 2011 Peter Hutterer <peter.hutterer@redhat.com> 1.14.0-1
- util-macros 1.14

* Tue Mar 15 2011 Peter Hutterer <peter.hutterer@redhat.com> 1.13.0-1
- util-macros 1.13

* Mon Feb 28 2011 Peter Hutterer <peter.hutterer@redhat.com> 1.12.0-1
- util-macros 1.12

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.11.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Mon Nov 01 2010 Peter Hutterer <peter.hutterer@redhat.com> 1.11.0-2
- util-macros 1.11

* Tue Jul 20 2010 Peter Hutterer <peter.hutterer@redhat.com> 1.10.0-2
- Spec file cleanup. Patch from Parag An. (#226646)

* Mon Jun 28 2010 Peter Hutterer <peter.hutterer@redhat.com> 1.10.0-1
- util-macros 1.10.0

* Thu Jun 24 2010 Peter Hutterer <peter.hutterer@redhat.com> 1.9.0-1
- util-macros 1.9.0

* Tue Jun 01 2010 Peter Hutterer <peter.hutterer@redhat.com> 1.8.0-1
- util-macros 1.8.0

* Tue May 18 2010 Peter Hutterer <peter.hutterer@redhat.com> 1.6.0-1
- util-macros 1.6.0

* Thu Feb 04 2010 Dave Airlie <airlied@redhat.com> 1.5.0-1
- util-macros 1.5.0

* Mon Dec 14 2009 Adam Jackson <ajax@redhat.com> 1.4.1-1
- util-macros 1.4.1

* Thu Sep 10 2009 Peter Hutterer <peter.hutterer@redhat.com> 1.3.0-1
- util-macros 1.3.0

* Mon Jul 27 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.2.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Sat Jul 18 2009 Peter Hutterer <peter.hutterer@redhat.com> 1.2.2-1
- util-macros 1.2.2

* Tue Apr 21 2009 Adam Jackson <ajax@redhat.com> 1.2.1-3
- Add Requires: for the things you inevitably require if you need this
  package.

* Thu Feb 26 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.2.1-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Wed Dec 17 2008 Adam Jackson <ajax@redhat.com> 1.2.1-1
- util-macros 1.2.1
- BuildArch: noarch

* Mon Aug 11 2008 Jason L Tibbitts III <tibbs@math.uh.edu> - 1.1.6-2
- Fix license tag.

* Wed Mar 05 2008 Adam Jackson <ajax@redhat.com> 1.1.6-1
- Update to 1.1.6

* Wed Feb 20 2008 Fedora Release Engineering <rel-eng@fedoraproject.org> - 1.1.5-2
- Autorebuild for GCC 4.3

* Fri Jan 05 2007 Adam Jackson <ajax@redhat.com> 1.1.5-1
- Update to 1.1.5

* Fri Dec 1 2006 Adam Jackson <ajax@redhat.com> 1.1.3-1
- Update to 1.1.3

* Thu Oct 12 2006 Adam Jackson <ajackson@redhat.com> 1.1.1-1.fc6
- Update to upstream 1.1.1.

* Sat Jul 15 2006 Mike A. Harris <mharris@redhat.com> 1.0.2-4.fc6
- Make dist tag usage a conditional (#198988)

* Thu Jul 13 2006 Kristian Høgsberg <krh@redhat.com> 1.0.2-3
- Bump for rawhide build.

* Thu Jul 13 2006 Kristian Høgsberg <krh@redhat.com> 1.0.2-2.fc5.aiglx
- Tag as 1.0.2-2.fc5.aiglx

* Wed Jul 12 2006 Jesse Keating <jkeating@redhat.com> 1.0.2-1.1
- rebuild

* Wed Jun 21 2006 Mike A. Harris <mharris@redhat.com> 1.0.2-1
- Update to util-macros-1.0.2 from X11R7.1
- Added COPYING, ChangeLog to file manifest.
- Use "make install" instead of makeinstall macro.
- Use setup -n instead of setup -c

* Fri Feb 10 2006 Jesse Keating <jkeating@redhat.com> 1.0.1-1.2
- bump again for double-long bug on ppc(64)

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> 1.0.1-1.1
- rebuilt for new gcc4.1 snapshot and glibc changes

* Fri Dec 23 2005 Mike A. Harris <mharris@redhat.com> 1.0.1-1
- Update to util-macros-1.0.1 from X11R7.

* Thu Dec 15 2005 Mike A. Harris <mharris@redhat.com> 1.0.0-1
- Update to util-macros-1.0.0 from the X11R7 RC4 release.

* Tue Dec 06 2005 Mike A. Harris <mharris@redhat.com> 0.99.2-1
- Update to util-macros-0.99.2 from the X11R7 RC3 release.

* Wed Oct 19 2005 Mike A. Harris <mharris@redhat.com> 0.99.1-1
- Update to util-macros-0.99.1 from the X11R7 RC1 release.
- Disable debuginfo package creation, as there are no ELF objects present.
- Add xorg-macros.m4 to file list.

* Wed Jul 13 2005 Mike A. Harris <mharris@redhat.com> 0.0.1-1
- Initial build
