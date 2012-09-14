# Define the kmod package name here.
%define kmod_name mhvtl

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-279.el6.%{_target_cpu}}

Summary: Virtual Tape Library device driver
Name: %{kmod_name}-kmod
%define real_version 2012-09-13
%define tar_version 1.4
Version: 1.4.4
Release: 1%{?dist}
License: GPLv2
Group: System Environment/Kernel
URL: http://sites.google.com/site/linuxvtl2/

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0: mhvtl-%{real_version}.tgz
Source5: GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the Virtual Tape Library device driver module for
Linux.  It is built to depend upon the specific ABI provided by a range
of releases of the same variant of the Linux kernel and not on any one
specific build.

%prep
%setup -n %{kmod_name}-%{tar_version}/kernel/
%{__cp} -a %{SOURCE5} .
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" modules_install M=$PWD
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Sep 14 2012 Dag Wieers <dag@wieers.com> - 1.4.4-1
- Updated to release 1.4-4 (2012-09-13).

* Thu Jun 21 2012 Dag Wieers <dag@wieers.com> - 1.3-1
- Updated to release 1.3 (2012-06-15).

* Thu Aug 05 2010 Dag Wieers <dag@wieers.com> - 0.18-11
- Initial el6 build of the kmod package.
