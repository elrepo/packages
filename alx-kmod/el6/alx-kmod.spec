# Define the kmod package name here.
%define kmod_name alx
%define src_name compat-wireless
%define src_version 2012-10-03-pc

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-279.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 0.0
Release: 1.20121003%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.linuxfoundation.org/collaborate/workgroups/networking/alx

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  http://www.orbit-lab.org/kernel/compat-wireless/%{src_name}-%{src_version}.tar.bz2
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

For more information on the development of this driver, please see:
http://www.linuxfoundation.org/collaborate/workgroups/networking/alx

%prep
%setup -q -n %{src_name}-%{src_version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
./scripts/driver-select alx
%{__make} KLIB=/lib/modules/%{kversion} KLIB_BUILD=/lib/modules/%{kversion}/build

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drivers/net/ethernet/atheros/alx/alx.ko \
    %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Mon Oct 15 2012 Philip J Perry <phil@elrepo.org> - 0.0-1
- Initial el6 build of the kmod package from nightly snapshot 2012-10-03-pc.
  [http://elrepo.org/bugs/view.php?id=306]
