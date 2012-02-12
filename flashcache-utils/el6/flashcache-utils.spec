Name: flashcache-utils	
Version: 0.0	
Release: 1%{?dist}
Summary: Utility packages for flashcache
Group: System Environment/Base
License: GPL v2
URL: https://github.com/facebook/flashcache

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
BuildRequires: rpm-build, redhat-rpm-config

# Sources.
Source0:  flashcache-utils-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: flashstat

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
The %{name} package contains utility packages for flashcache including
flashcache_create, flashcache_load and flashcache_destroy. It is expected
that the majority of users can use these utilities instead of dmsetup.

%prep
%setup -q -n flashcache-utils-%{version}
%{__cp} -a %{SOURCE5} .
%{__cp} -a %{SOURCE10} .

%build
%{__make} -s %{?_smp_mflags}

%install
%{__make} -s install DESTDIR="%{buildroot}"
%{__install} -m 755 flashstat %{buildroot}/sbin/
%{__install} -d %{buildroot}%{_defaultdocdir}/%{name}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/%{name}/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/sbin/flashcache_create
/sbin/flashcache_destroy
/sbin/flashcache_load
/sbin/flashstat
/usr/share/doc/flashcache-utils/GPL-v2.0.txt

%changelog
* Sun Feb 12 2012 Akemi Yagi <toracat@elrepo.org> - 0.0-1
- Initial build for EL6.
