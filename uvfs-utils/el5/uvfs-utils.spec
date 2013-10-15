# $Id$
# Authority: dag

%define _sbindir /sbin

%define real_name uvfs

Summary: uvfs/pmfs utilities
Name: uvfs-utils
Version: 2.0.6
Release: 1%{?dist}
License: GPLv2
Group: System Environment/Kernel
URL: http://www.sciencething.org/geekthings/UVFS_README.html

Source: http://dl.sf.net/project/uvfs/uvfs/%{version}/uvfs_%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
Utility to signal the filesystem to shutdown the file system or provide
debugging information.

%prep
%setup -n %{real_name}_%{version}

%build
%{__make} uvfs_signal

%install
%{__rm} -rf %{buildroot}
%{__install} -Dm0755 uvfs_signal %{buildroot}%{_sbindir}/uvfs_signal

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%doc GPL LGPL README
/sbin/uvfs_signal

%changelog
* Tue Oct 15 2013 Dag Wieers <dag@wieers.com> - 2.0.6-1
- Updated to release 2.0.6.

* Thu Aug 02 2012 Dag Wieers <dag@wieers.com> - 2.0.5-1
- Initial build of the kmod packages.
