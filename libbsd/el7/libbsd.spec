Name:		libbsd
Version:	0.6.0
Release:	3%{?dist}
Summary:	Library providing BSD-compatible functions for portability
URL:		http://libbsd.freedesktop.org/
License:	BSD and ISC and Copyright only and Public Domain
Group:		System Environment/Libraries
# BuildRoot tag necessary for EL5 only:
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
Source0:	http://libbsd.freedesktop.org/releases/libbsd-%{version}.tar.xz

%description
libbsd provides useful functions commonly found on BSD systems, and
lacking on others like GNU systems, thus making it easier to port
projects with strong BSD origins, without needing to embed the same
code over and over again on each project.

%package devel
Summary:	Development files for libbsd
Group:		Development/Libraries
Requires:	libbsd = %{version}-%{release}
Requires:	pkgconfig

%description devel
Development files for the libbsd library.

%package ctor-static
Summary:	Development files for libbsd
Group:		Development/Libraries
Requires:	libbsd = %{version}-%{release}
Requires:	pkgconfig

%description ctor-static
The libbsd-ctor static library is required if setproctitle() is to be used
when libbsd is loaded via dlopen() from a threaded program.  This can be
configured using "pkg-config --libs libbsd-ctor".
# See the libbsd mailing list message by Guillem Jover on Jul 14 2013:
#     http://lists.freedesktop.org/archives/libbsd/2013-July/000091.html

%prep
%setup -q

%configure

%build
make CFLAGS="%{optflags}" %{?_smp_mflags} \
     libdir=%{_libdir} \
     usrlibdir=%{_libdir} \
     exec_prefix=%{_prefix}

%check
make check

%install
rm -rf %{buildroot}   # necessary for EL5 only
make libdir=%{_libdir} \
     usrlibdir=%{_libdir} \
     exec_prefix=%{_prefix} \
     DESTDIR=%{buildroot} \
     install

# don't want static library or libtool archive
rm %{buildroot}%{_libdir}/%{name}.a
rm %{buildroot}%{_libdir}/%{name}.la

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%doc COPYING README TODO ChangeLog
%{_libdir}/%{name}.so.*

%files devel
%{_mandir}/man3/*.3.gz
%{_mandir}/man3/*.3bsd.gz
%{_includedir}/bsd
%{_libdir}/%{name}.so
%{_libdir}/pkgconfig/%{name}.pc
%{_libdir}/pkgconfig/%{name}-overlay.pc

%files ctor-static
%{_libdir}/%{name}-ctor.a
%{_libdir}/pkgconfig/%{name}-ctor.pc

%changelog
* Sun Jun 08 2014 Rob Mokkink <rob@mokkinksystems.com> - 0.6.0-3
- Rebuild Fedora 20 for Elrepo

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.6.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Sun Jul 14 2013 Eric Smith <brouhaha@fedoraproject.org> - 0.6.0-1
- Update to latest upstream release. Remove patch 0.
- Added ctor-static subpackage.

* Sun Jul 07 2013 Eric Smith <brouhaha@fedoraproject.org> - 0.5.2-3
- Still having problems with setproctitle(), bug #981799, upstream
  freedesktop.org bug #66679. Added patch to noop out setproctitle().

* Tue Jun 11 2013 Eric Smith <brouhaha@fedoraproject.org> - 0.5.2-2
- Added check section.
- Add BuildRoot for EL5.

* Mon Jun 10 2013 Eric Smith <brouhaha@fedoraproject.org> - 0.5.2-1
- Update to latest upstream release. Remove patch 0.

* Thu Jun 06 2013 Eric Smith <brouhaha@fedoraproject.org> - 0.5.1-2
- Add patch to avoid calling clearenv() in setproctitle.c, bug #971513.

* Tue Jun 04 2013 Eric Smith <brouhaha@fedoraproject.org> - 0.5.1-1
- Update to latest upstream release.

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.4.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Thu Jul 19 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.4.2-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Sun Jul 08 2012 Eric Smith <eric@brouhaha.com> - 0.4.2-1
- Update to latest upstream release.
- No longer need to change encoding of flopen(3) man page.

* Sun Jun 03 2012 Eric Smith <eric@brouhaha.com> - 0.4.1-1
- Update to latest upstream release.

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 0.3.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Sat Oct 08 2011 Eric Smith <eric@brouhaha.com> - 0.3.0-1
- Update to latest upstream release.
- Removed Patch0, fixed upstream.
- Removed BuildRoot, clean, defattr.

* Fri Jan 29 2010 Eric Smith <eric@brouhaha.com> - 0.2.0-3
- changes based on review by Sebastian Dziallas

* Fri Jan 29 2010 Eric Smith <eric@brouhaha.com> - 0.2.0-2
- changes based on review comments by Jussi Lehtola and Ralf Corsepious

* Thu Jan 28 2010 Eric Smith <eric@brouhaha.com> - 0.2.0-1
- initial version
