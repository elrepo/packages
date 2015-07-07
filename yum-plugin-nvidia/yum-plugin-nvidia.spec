%define pluginhome /usr/lib/yum-plugins

Name:    yum-plugin-nvidia
Version: 1.0.0
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
Requires:	nvidia-kmod >= 352.21-3
Requires:	nvidia-x11-drv >= 352.21-3

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
%{__python} -c "import compileall; compileall.compile_dir('%{buildroot}%{pluginhome}', 1)"

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root)
%doc COPYING README
%config(noreplace) %{_sysconfdir}/yum/pluginconf.d/nvidia.conf
%{pluginhome}/nvidia.*

%changelog
* Tue Jul 07 2015 Philip J Perry <phil@elrepo.org> - 1.0.0-1
- Initial release of the package
