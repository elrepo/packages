# Define the kmod package name here.
%define kmod_name rt3090sta

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-71.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 2.4.0.4
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.ralinktech.com.tw/data/drivers/20101216_RT3090_LinuxSTA_V2.4.0.4_WiFiBTCombo_DPO.tar.bz2

BuildRequires: redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.bz2
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
%{__cp} -a %{SOURCE5} .
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
cp Makefile Makefile.distro
grep -vE '#|tftpboot' Makefile.distro > Makefile

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make}

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
%{__install} -d ${INSTALL_MOD_PATH}/lib/modules/%{kversion}/${INSTALL_MOD_DIR}
%{__cp} -a os/linux/rt3090sta.ko ${INSTALL_MOD_PATH}/lib/modules/%{kversion}/${INSTALL_MOD_DIR}
%{__install} -d ${INSTALL_MOD_PATH}/etc/Wireless/RT2860STA/
%{__install} RT2860STA.dat ${INSTALL_MOD_PATH}/etc/Wireless/RT2860STA/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} a+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Jan 20 2011 Akemi Yagi <toracat@elrepo.org> - 2.4.0.4-1.el6.elrepo
- Source from the manufacturer used.

* Tue Jan 18 2011 Akemi Yagi <toracat@elrepo.org> - 2.1.0-1.el6.elrepo
- Initial el6 build of the kmod package.
