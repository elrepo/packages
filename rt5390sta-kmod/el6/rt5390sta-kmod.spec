# Define the kmod package name here.
%define kmod_name rt5390sta

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-71.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 2.4.0.4
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: Ralink %{kmod_name} driver module
URL:     http://www.ralinktech.com.tw/data/drivers/2010_1216_RT5390_LinuxSTA_V2.4.0.4_WiFiBTCombo_DPO.tar.bz2

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  2010_1216_RT5390_LinuxSTA_V2.4.0.4_WiFiBTCombo_DPO.tar.bz2
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s) for the
RaLink RT5390 wireless 802.11n PCIe series network adapters.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
%{__cp} -a 2010_1216_RT5390_LinuxSTA_V2.4.0.4_WiFiBTCombo_DPO _kmod_build_
%{__cp} -a %{SOURCE5} _kmod_build_/
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > _kmod_build_/kmod-%{kmod_name}.conf
%{__perl} -pi -e 's|(.*tftpboot$)|#$1|g' _kmod_build_/Makefile

%build
pushd _kmod_build_
%{__make} LINUX_SRC=%{_usrsrc}/kernels/%{kversion}
popd

%install
pushd _kmod_build_
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} os/linux/rt5390sta.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/Wireless/RT2860STA/
%{__install} RT2860STA.dat %{buildroot}%{_sysconfdir}/Wireless/RT2860STA/
%{__install} RT2860STACard.dat %{buildroot}%{_sysconfdir}/Wireless/RT2860STA/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Install docs
%{__install} *.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README_STA_pci %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
popd
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Wed Jul 27 2011 Philip J Perry <phil@elrepo.org> - 2.4.0.4-1.el6.elrepo
- Initial el6 build of the kmod package.
  http://elrepo.org/bugs/view.php?id=167
