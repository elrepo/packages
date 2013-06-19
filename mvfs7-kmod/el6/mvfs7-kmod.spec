# Define the kmod package name here.
%define kmod_name mvfs7
%define real_name mvfs

# Disable stripping
%define __os_install_post %{nil}

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-358.el6.%{_target_cpu}}

%define ratl_vendor_ver 604
%ifarch x86_64
%define ratl_compat32 -DRATL_COMPAT32
%else
%define ratl_compat32 %nil
%endif

Summary: Multi-version file system (MVFS), part of IBM Rational ClearCase
Name: %{kmod_name}-kmod
Version: 7.1.2.10
Release: 1%{?dist}
License: GPLv2
Group: System Environment/Kernel
URL: http://github.com/dagwieers/mvfs7/

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0: https://github.com/dagwieers/mvfs7/archive/mvfs7-%{version}.tar.gz
Source5: GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the Multi-version file system (MVFS) which is
part of the IBM Rational ClearCase software.

For support, please visit http://www.ibm.com/software/support

%prep
%setup -n %{kmod_name}-%{version}
%{__cp} -a %{SOURCE5} .
echo "override %{real_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%{__cat} <<EOF >mvfs_param.mk.config
CONFIG_MVFS=m
RATL_EXTRAFLAGS := -DRATL_REDHAT -DRATL_VENDOR_VER=%{ratl_vendor_ver} -DRATL_EXTRA_VER=0 %{ratl_compat32}
LINUX_KERNEL_DIR=/lib/modules/%{kversion}/build
EOF

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=kernel/fs/%{real_name}
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" modules_install M=$PWD
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Mon Jun 03 2013 Dag Wieers <dag@wieers.com> - 7.1.2.10-1
- Updated to release 7.1.2.10.
- Renamed to kmod-mvfs7.

* Mon May 27 2013 Dag Wieers <dag@wieers.com> - 7.1.2.2-1
- Initial el6 build of the kmod package.
