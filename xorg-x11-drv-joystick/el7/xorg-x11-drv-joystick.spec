%define tarball xf86-input-joystick
%define moduledir %(pkg-config xorg-server --variable=moduledir )
%define driverdir	%{moduledir}/input

%define cvsdate xxxxxxx

Summary:   Xorg X11 joystick input driver
Name:      xorg-x11-drv-joystick
Version: 1.6.2
Release: 1%{?dist}
URL:       http://www.x.org
License:   MIT/X11
Group:     User Interface/X Hardware Support
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

Source0:   ftp://ftp.x.org/pub/individual/driver/%{tarball}-%{version}.tar.bz2

ExclusiveArch: %{ix86} x86_64 ia64 ppc alpha sparc sparc64

BuildRequires: pkgconfig
BuildRequires: xorg-x11-server-devel >= 1.0.99.901

Requires:  xorg-x11-server-Xorg >= 1.0.99.901

%description 
X.Org X11 joystick input driver.

%prep
%setup -q -n %{tarball}-%{version}

%build
%configure --disable-static
make

%install
rm -rf $RPM_BUILD_ROOT

make install DESTDIR=$RPM_BUILD_ROOT

# FIXME: Remove all libtool archives (*.la) from modules directory.  This
# should be fixed in upstream Makefile.am or whatever.
find $RPM_BUILD_ROOT -regex ".*\.la$" | xargs rm -f --

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%dir %{moduledir}
%dir %{driverdir}
%{driverdir}/joystick_drv.so
#%dir %{_mandir}
#%{_mandir}/man4/.4*
# Major kludge for unpackaged files
/usr/include/xorg/joystick-properties.h
%{_libdir}/pkgconfig/xorg-joystick.pc
/usr/share/man/man4/joystick.4.gz

%changelog
* Sun Jan 18 2015 Levente Farkas <lfarkas@lfarkas.org> 1.6.2-1
- Update to 1.6.2

* Wed Nov  9 2011 Phil Schaffner <pschaff2@verizon.net> 1.5.0-1
- Update to 1.5.0 from 1.1.0.

* Wed Jul 12 2006 Jesse Keating <jkeating@redhat.com> - sh: line 0: fg: no job control
- rebuild

* Sun Apr  9 2006 Adam Jackson <ajackson@redhat.com> 1.1.0-1
- Update to 1.1.0 from 7.1RC1.

* Fri Feb 10 2006 Jesse Keating <jkeating@redhat.com> - 1.0.0.5-1.2
- bump again for double-long bug on ppc(64)

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> - 1.0.0.5-1.1
- rebuilt for new gcc4.1 snapshot and glibc changes

* Wed Jan 18 2006 Mike A. Harris <mharris@redhat.com> 1.0.0.5-1
- Updated xorg-x11-drv-joystick to version 1.0.0.5 from X11R7.0

* Tue Dec 20 2005 Mike A. Harris <mharris@redhat.com> 1.0.0.4-1
- Updated xorg-x11-drv-joystick to version 1.0.0.4 from X11R7 RC4
- Removed 'x' suffix from manpage dirs to match RC4 upstream.

* Wed Nov 16 2005 Mike A. Harris <mharris@redhat.com> 1.0.0.2-1
- Updated xorg-x11-drv-joystick to version 1.0.0.2 from X11R7 RC2

* Fri Nov 4 2005 Mike A. Harris <mharris@redhat.com> 1.0.0.1-1
- Updated xorg-x11-drv-joystick to version 1.0.0.1 from X11R7 RC1
- Fix *.la file removal.

* Fri Sep 2 2005 Mike A. Harris <mharris@redhat.com> 1.0.0-0
- Initial spec file for joystick input driver generated automatically
  by my xorg-driverspecgen script.
