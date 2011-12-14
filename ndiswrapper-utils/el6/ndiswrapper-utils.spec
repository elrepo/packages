# $Id$
# Authority: dag

%define real_name ndiswrapper

Summary: Ndiswrapper utilities
Name: ndiswrapper-utils
Version: 1.57
Release: 0.1.rc1%{?dist}
License: GPLv2
Group: System Environment/Kernel
URL: http://ndiswrapper.sourceforge.net/

#Source: http://heanet.dl.sourceforge.net/project/ndiswrapper/stable/%{version}/ndiswrapper-%{version}.tar.gz
Source: http://heanet.dl.sourceforge.net/project/ndiswrapper/testing/1.57-rc1/ndiswrapper-1.57rc1.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
This package provides the utilities to use the ndiswrapper kernel module.

%prep
%setup -n %{real_name}-%{version}

### Disable driver build
echo "install:" >driver/Makefile

%build
%{__make}

%install
%{__rm} -rf %{buildroot}
%{__make} install DESTDIR="%{buildroot}"

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%doc AUTHORS ChangeLog
%doc %{_mandir}/man8/loadndisdriver.8*
%doc %{_mandir}/man8/ndiswrapper.8*
/sbin/loadndisdriver
%{_sbindir}/ndiswrapper
%{_sbindir}/ndiswrapper-buginfo

%changelog
* Wed Dec 14 2011 Philip J Perry <phil@elrepo.org> - 1.57-0.1.rc1.el6.elrepo
- Update to 1.57rc1.

* Wed Jun 16 2010 Dag Wieers <dag@elrepo.org> - 1.56-1
- Updated to release 1.56.

* Fri Oct 23 2009 Alan Bartlett <ajb@elrepo.org> - 1.54-4
- Revised the kmodtool file and this spec file.

* Fri Oct 09 2009 Alan Bartlett <ajb@elrepo.org> - 1.54-3
- Revised the kmodtool file and this spec file.

* Mon Aug 17 2009 Alan Bartlett <ajb@elrepo.org> - 1.54-2
- Revised the kmodtool file and this spec file.

* Thu May 07 2009 Alan Bartlett <ajb@elrepo.org> - 1.54-1
- Initial build of the kmod packages.
