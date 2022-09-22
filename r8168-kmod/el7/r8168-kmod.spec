# The current upstream tarball is named 0012-r8168-8.046.00.tar.bz2
# so we adjust the Source0 line, below.

# Define the kmod package name here.
%define kmod_name r8168

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1160.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 8.050.03
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.realtek.com/

BuildRequires: redhat-rpm-config, perl
ExclusiveArch: x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.bz2
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh
Source20: ELRepo-Makefile-%{kmod_name}

# Patches.
Patch0: ELRepo-r8168-%{version}.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s) for Realtek RTL8168/RTL8111,
RTL8168B/RTL8111B, RTL8168C/RTL8111C, RTL8168D/RTL8111D, RTL8168E/RTL8111E
and RTL8168F/RTL8111F Gigabit Ethernet controllers with the PCI-Express interface.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}
%patch0 -p1
%{__rm} -f src/Makefile*
%{__cp} -a %{SOURCE20} src/Makefile
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "blacklist r8169" > blacklist-r8169.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD/src

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} blacklist-r8169.conf %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Strip the modules(s).
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

# Sign the modules(s).
%if %{?_with_modsign:1}%{!?_with_modsign:0}
# If the module signing keys are not defined, define them here.
%{!?privkey: %define privkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.priv}
%{!?pubkey: %define pubkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.der}
for module in $(find %{buildroot} -type f -name \*.ko);
do %{__perl} /usr/src/kernels/%{kversion}/scripts/sign-file \
    sha256 %{privkey} %{pubkey} $module;
done
%endif

%clean
%{__rm} -rf %{buildroot}

%changelog
* Wed Sep 21 2022 Akemi Yagi <toracat@elrepo.org> - 8.050.03-1
- Updated to version 8.050.03
- Enabled -DCONFIG_DYNAMIC_ASPM

* Sun Aug 08 2021 Philip J Perry <phil@elrepo.org> - 8.049.02-1
- Updated to version 8.049.02
- Built against RHEL 7.9 kernel
- Enabled -DCONFIG_R8168_VLAN, -DCONFIG_ASPM, -DENABLE_S5WOL, -DENABLE_EEE,
          -DENABLE_USE_FIRMWARE_FILE

* Sun Mar 01 2020 Philip J Perry <phil@elrepo.org> - 8.048.00-1
- Updated to version 8.048.00

* Fri Nov 22 2019 Akemi Yagi <toracat@elrepo.org> - 8.047.05-1
- Updated to version 8.047.05

* Thu Sep 26 2019 Akemi Yagi <toracat@elrepo.org> - 8.047.04-1
- Updated to version 8.047.04
- Built against RHEL 7.7 kernel

* Thu Sep 06 2018 Alan Bartlett <ajb@elrepo.org> - 8.046.00-1
- Updated to version 8.046.00

* Sat May 12 2018 Akemi Yagi <toracat@elrepo.org> - 8.045.08-2
- Rebuilt against RHEL 7.5 kernel.

* Sun Sep 17 2017 Philip J Perry <phil@elrepo.org> - 8.045.08-1
- Updated to version 8.045.08
- Rebuilt against RHEL7.4 kernel.

* Tue Oct 18 2016 Alan Bartlett <ajb@elrepo.org> - 8.043.01-1
- Updated to version 8.043.01

* Mon Jul 27 2015 Alan Bartlett <ajb@elrepo.org> - 8.040.00-1
- Updated to version 8.040.00

* Wed Oct 08 2014 Alan Bartlett <ajb@elrepo.org> - 8.039.00-1
- Updated to version 8.039.00

* Thu Jun 12 2014 Alan Bartlett <ajb@elrepo.org> - 8.038.00-1
- Updated to version 8.038.00

* Thu Jun 12 2014 Alan Bartlett <ajb@elrepo.org> - 8.037.00-3
- Updated to the GA RHEL7 base kernel.

* Fri May 23 2014 Philip J Perry <phil@elrepo.org> - 8.037.00-2
- Fix blacklist location.
- Corrected the kmodtool file.

* Tue Jan 14 2014 Alan Bartlett <ajb@elrepo.org> - 8.037.00-1
- Initial el7 build of the kmod package.
