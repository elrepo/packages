# Define the kmod package name here.
%define kmod_name ixgbevf

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-696.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 4.3.2
Release: 2%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.intel.com/

BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64 i686

# Sources.
Source0:  %{kmod_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}
%{__gzip} %{kmod_name}.7
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
pushd src >/dev/null
%{__make} KSRC=%{_usrsrc}/kernels/%{kversion}
popd >/dev/null

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} pci.updates %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -d %{buildroot}%{_mandir}/man7/
%{__install} %{kmod_name}.7.gz %{buildroot}%{_mandir}/man7/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Nov 03 2017 Akemi Yagi <toracat@elrepo.org> - 4.3.2-2
- Updated version to 4.3.2
- Built against the EL6.9 kernel 2.6.32-696
- Removed requirement for kmod-ixgbe
- Build both x86_64 and i686

* Sun Feb 01 2015 Alan Bartlett <ajb@elrepo.org> - 2.16.1-1
- Updated version to 2.16.1

* Sat Oct 04 2014 Alan Bartlett <ajb@elrepo.org> - 2.15.3-1
- Updated version to 2.15.3

* Thu May 01 2014 Alan Bartlett <ajb@elrepo.org> - 2.14.2-1
- Updated version to 2.14.2

* Sat Mar 01 2014 Alan Bartlett <ajb@elrepo.org> - 2.12.1-1
- Updated version to 2.12.1

* Thu Feb 27 2014 Alan Bartlett <ajb@elrepo.org> - 2.11.3-1
- Initial el6 build of the kmod package.
