%define pluginhome /usr/lib/yum-plugins

Name:    yum-plugin-nvidia
Version: 1.0.2
Release: 1%{?dist}
Group:   Development/Tools
License: GPLv2
Summary: Yum plugin to prevent update of NVIDIA drivers on unsupported hardware
URL:     https://github.com/elrepo/packages/tree/master/yum-plugin-nvidia

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
BuildArch: noarch

Source0:   %{name}-%{version}.tar.bz2

Requires:	python >= 2.4
Requires:	yum >= 3.2.22

BuildRequires: python-devel >= 2.4

%description
A yum plugin to prevent install or update of NVIDIA drivers on unsupported hardware.

NVIDIA occasionally drops support for older hardware from the current driver release.
This plugin prevents yum from updating NVIDIA drivers on older hardware where support
for that hardware has been dropped.

%prep
%setup -q

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}%{_sysconfdir}/yum/pluginconf.d/
%{__mkdir_p} %{buildroot}%{pluginhome}
%{__install} -m 0644 nvidia.conf %{buildroot}%{_sysconfdir}/yum/pluginconf.d/
%{__install} -m 0644 nvidia.py %{buildroot}%{pluginhome}/nvidia.py

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root)
%doc COPYING README
%config(noreplace) %{_sysconfdir}/yum/pluginconf.d/nvidia.conf
%{pluginhome}/nvidia.py
%{pluginhome}/nvidia.pyc
%{pluginhome}/nvidia.pyo

%changelog
* Wed Jul 29 2015 Philip J Perry <phil@elrepo.org> - 1.0.2-1
- Make default output less verbose
- Remove requires for nvidia drivers

* Mon Jul 20 2015 Philip J Perry <phil@elrepo.org> - 1.0.1-1
- Some minor code cleanup

* Tue Jul 07 2015 Philip J Perry <phil@elrepo.org> - 1.0.0-2
- Let brp-python-bytecompile handle the byte compile

* Tue Jul 07 2015 Philip J Perry <phil@elrepo.org> - 1.0.0-1
- Initial release of the package
