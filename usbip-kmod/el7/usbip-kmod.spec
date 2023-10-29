# Define the kmod package name here.
%define kmod_name usbip

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1062.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 1.0.1
Release: 4%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.kernel.org/

BuildRequires: perl
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh

# Patches.
Patch0: ELRepo-%{kmod_name}-Makefile.patch
Patch1: ELRepo-usbip_common.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the usbip-core, usbip-host and vhci-hcd kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}
%patch0 -p1
%patch1 -p1
echo "override %{kmod_name}-core * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override %{kmod_name}-host * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override vhci-hcd * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-core.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-host.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} vhci-hcd.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# strip the modules(s)
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

# Sign the modules(s)
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
* Mon Sep 02 2019 Akemi Yagi <toracat@elrepo.org> - 1.0.1-4
- Built against RHEL 7.7 kernel

* Sun Nov 11 2018 Akemi Yagi <toracat@elrepo.org> - 1.0.1-3
- Rebuilt against RHEL 7.6 kernel
- Added usbip_common_h_7_6.patch

* Tue May 01 2018 Philip J Perry <phil@elrepo.org> - 1.0.1-2
- Rebuilt against RHEL 7.5 kernel
- Update to kernel 3.18.107
- Fixes CVE-2017-16911, CVE-2017-16912, CVE-2017-16913, CVE-2017-16914
- [http://elrepo.org/bugs/view.php?id=845]

* Thu Nov 23 2017 Akemi Yagi <toracat@elrepo.org> - 1.0.1-1
- Initial build for el7.
- Backported from kernel 3.18.83

* Fri Dec 13 2013 Philip J Perry <phil@elrepo.org> - 1.0.0-2
- Updated to linux-3.12.4 sources.
- Fixed backported structures

* Fri Dec 13 2013 Alan Bartlett <ajb@elrepo.org> - 1.0.0-1
- Backported code from the linux-3.12.2 sources.

* Mon Nov 18 2013 Alan Bartlett <ajb@elrepo.org> - 0.0-1
- Initial el6 build of the kmod package.
- [http://elrepo.org/bugs/view.php?id=428]
