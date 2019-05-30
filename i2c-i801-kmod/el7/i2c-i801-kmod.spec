# Define the kmod package name here.
%define kmod_name i2c-i801

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-957.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 0.0
Release: 6%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.kernel.org/

BuildRequires: perl
BuildRequires: redhat-rpm-config, perl
ExclusiveArch: x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh

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
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

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
* Thu May 30 2019 Philip J Perry <phil@elrepo.org> - 0.0-6.el7_6.elrepo
- Updated to kernel-4.19.46
- Backports support for modern chipsets (Kaby Lake, Gemini Lake, Cannon Lake,
  Cedar Fork, Ice Lake, Comet Lake)

* Sun Nov 11 2018 Akemi Yagi <toracat@elrepo.org> - 0.0-5.el7_6.elrepo
- Rebuilt against RHEL 7.6 kernel

* Fri Jun 15 2018 Akemi Yagi <toracat@elrepo.org> - 0.0-4.el7_5.elrepo
- Rebuilt against RHEL 7.5 kernel

* Sat Dec 03 2016 Philip J Perry <phil@elrepo.org> - 0.0-3
- Updated to kernel-4.4.36
- Built against RHEL7.3 kernel
- Backports support for Braswell and Wildcat Point ICH's

* Sat Jan 16 2016 Philip J Perry <phil@elrepo.org> - 0.0-2
- Updated to kernel-4.1.15

* Fri Jan 15 2016 Alan Bartlett <ajb@elrepo.org> - 0.0-1
- Initial el7 build of the kmod package.
