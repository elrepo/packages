# Define the utils package name here.
%define utils_name flashcache

Name: %{utils_name}-utils
Version: 3.1.2
Release: 1%{?dist}
Summary: Utility packages for flashcache
Group: System Environment/Base
License: GPLv2
URL: https://github.com/facebook/flashcache

BuildRequires: redhat-rpm-config

# Sources.
Source0:  %{utils_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt

# Patches.
Patch0: ELRepo-%{utils_name}.patch

Requires: sudo

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
The %{name} package contains utility packages for flashcache including
flashcache_create, flashcache_load and flashcache_destroy. It is expected
that the majority of users can use these utilities instead of dmsetup.

%prep
%setup -q -n %{utils_name}-%{version}
%patch0 -p1

%build
%{__make} -C src/utils

%install
%{__install} -d %{buildroot}/sbin
%{__install} -m 755 src/utils/flashcache_create %{buildroot}/sbin/
%{__install} -m 755 src/utils/flashcache_destroy %{buildroot}/sbin/
%{__install} -m 755 src/utils/flashcache_load %{buildroot}/sbin/
%{__install} -m 755 src/utils/flashcache_scan %{buildroot}/sbin/
%{__install} -m 755 src/utils/flashcache_setioctl %{buildroot}/sbin/
%{__install} -m 755 src/utils/get_agsize %{buildroot}/sbin/
%{__install} -m 755 utils/flashstat %{buildroot}/sbin/
%{__install} -d -m 755 %{buildroot}/usr/lib/ocf/resource.d/
%{__install} -m 755 src/ocf/flashcache %{buildroot}/usr/lib/ocf/resource.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/%{name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/%{name}-%{version}/

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
/sbin/flashcache_create
/sbin/flashcache_destroy
/sbin/flashcache_load
/sbin/flashcache_scan
/sbin/flashcache_setioctl
/sbin/get_agsize
/sbin/flashstat
/usr/lib/ocf/resource.d/flashcache
%doc /usr/share/doc/%{name}-%{version}/GPL-v2.0.txt

%changelog
* Sat Jul 26 2014 Alan Bartlett <ajb@elrepo.org> - 3.1.2-1
- Updated to the flashcache-3.1.2 sources.
- Re-wrote this specification file.

* Thu Mar 28 2013 Akemi Yagi <toracat@elrepo.org> - 0.0-4.1
- added Requires: sudo [http://elrepo.org/bugs/view.php?id=356]

* Mon Dec 24 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-4
- Updated to flashcache-stable_v2.
- Provides: /sbin/flashcache_create flashcache_destroy flashcache_load flashcache_scan 
  flashcache_setioctl flashstat get_agsize and /usr/lib/ocf/resource.d/flashcache

* Sat Jun 02 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-3.1
- Update to flashcache submitted by mvdlande.

* Sat May 12 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-3
- Added /sbin/flashcache_setioctl and /usr/lib/ocf/resource.d/flashcache

* Sat Mar 03 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-2
- Packaging style now conforms to the ELRepo standard. [Alan Bartlett]

* Sun Feb 12 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-1
- Initial build for EL6.
