%define pluginhome /usr/lib/yum-plugins

Name:    yum-plugin-elrepo
Version: 7.5.2
Release: 1%{?dist}
Group:   Development/Tools
License: GPLv2
Summary: Yum plugin to exclude kmod packages where the required kernel is missing
URL:     https://github.com/elrepo/packages/tree/master/yum-plugin-elrepo

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
BuildArch: noarch

Source0:   %{name}-%{version}.tar.gz

Requires:	python >= 2.4
Requires:	yum >= 3.2.22

BuildRequires: python-devel >= 2.4

%description
A yum plugin to exclude kmod packages from the yum transaction set which
require kernels that are not yet available.

%prep
%setup -q

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}%{_sysconfdir}/yum/pluginconf.d/
%{__mkdir_p} %{buildroot}%{pluginhome}
%{__install} -m 0644 elrepo.conf %{buildroot}%{_sysconfdir}/yum/pluginconf.d/
%{__install} -m 0644 elrepo.py %{buildroot}%{pluginhome}/elrepo.py

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root)
%doc COPYING README
%config(noreplace) %{_sysconfdir}/yum/pluginconf.d/elrepo.conf
%{pluginhome}/elrepo.py
%{pluginhome}/elrepo.pyc
%{pluginhome}/elrepo.pyo

%changelog
* Wed Apr 15 2020 Jason A. Donenfeld <Jason@zx2c4.com> - 7.5.2-1
- Reduce verbosity.

* Tue Nov 20 2018 Philip J Perry <phil@elrepo.org> - 7.5.1-1
- Remove workaround for missing older kernels

* Sun Nov 18 2018 Philip J Perry <phil@elrepo.org> - 7.5.0-1
- Initial release of the package
