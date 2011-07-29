# Define the kmod package name here.
%define kmod_name rt3070sta

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-71.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 2.5.0.1
Release: 2%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.ralinktech.com

BuildRequires: redhat-rpm-config
BuildRequires: perl
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  2011_0107_RT3070_RT3370_Linux_STA_v2.5.0.1_DPO.tgz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Patches.
Patch0: rt3070sta-unspecified-license.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s) for the
RaLink RT8070/RT3070/RT3370 wireless USB series network adapters.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
%patch0 -p0
%{__cp} -a 2011_0107_RT3070_RT3370_Linux_STA_v2.5.0.1_DPO _kmod_build_
%{__cp} -a %{SOURCE5} _kmod_build_/
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > _kmod_build_/kmod-%{kmod_name}.conf

%{__perl} -pi -e 's|(.*tftpboot$)|#$1|g' _kmod_build_/Makefile
%{__perl} -pi -e 's|HAS_WPA_SUPPLICANT=n|HAS_WPA_SUPPLICANT=y|g;
                  s|HAS_NATIVE_WPA_SUPPLICANT_SUPPORT=n|HAS_NATIVE_WPA_SUPPLICANT_SUPPORT=y|g' \
    _kmod_build_/os/linux/config.mk

%build
pushd _kmod_build_
%{__make} LINUX_SRC=%{_usrsrc}/kernels/%{kversion}
popd

%install
pushd _kmod_build_
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} os/linux/rt3070sta.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/Wireless/RT2870STA/
%{__install} RT2870STA.dat %{buildroot}%{_sysconfdir}/Wireless/RT2870STA/
%{__install} RT2870STACard.dat %{buildroot}%{_sysconfdir}/Wireless/RT2870STA/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Install docs
%{__install} *.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README_STA_usb %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
popd
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Sat Mar 19 2011 Philip J Perry <phil@elrepo.org> - 2.5.0.1-2.el6.elrepo
- Fix module license 'unspecified' taints kernel [rt3070sta-unspecified-license.patch]
  [http://elrepo.org/bugs/view.php?id=121]

* Fri Mar 18 2011 Philip J Perry <phil@elrepo.org> - 2.5.0.1-1.el6.elrepo
- Initial el6 build of the kmod package.
