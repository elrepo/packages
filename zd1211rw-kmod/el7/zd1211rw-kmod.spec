# Define the kmod package name here.
%define kmod_name zd1211rw

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1127.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 1.0
Release: 8%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.kernel.org/

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
* Sat Apr 04 2020 Philip J Perry <phil@elrepo.org> - 1.0-8
- Rebuilt against RHEL 7.8 kernel
- Backported from kernel-5.3.18

* Sun Sep 15 2019 Akemi Yagi <toracat@elrepo.org> - 1.0-7
- Rebuilt against RHEL 7.7 kernel

* Sat Jul 21 2018 Philip J Perry <phil@elrepo.org> - 1.0-6
- Backported from kernel-4.14.56 for RHEL-7.5

* Sun Jul 30 2017 Philip J Perry <phil@elrepo.org> - 1.0-5
- Backported from kernel-4.11.12 for RHEL-7.4

* Sun Nov 06 2016 Philip J Perry <phil@elrepo.org> - 1.0-4
- Backported from kernel-4.7.10 for RHEL-7.3

* Wed Dec 16 2015 Philip J Perry <phil@elrepo.org> - 1.0-3
- Updated to kernel-4.1.15
- Rebuilt against RHEL 7.2 kernel

* Thu Mar 05 2015 Philip J Perry <phil@elrepo.org> - 1.0-2
- Rebuilt against RHEL 7.1 kernel

* Fri Sep 12 2014 Alan Bartlett <ajb@elrepo.org> - 1.0-1
- Initial el7 build of the kmod package.
- Backported from kernel-3.10.50
