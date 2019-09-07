# Define the kmod package name here.
%define kmod_name 8188eu

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1062.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 5.2.2.4
Release: 1.20190907git%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     https://github.com/lwfinger/rtl8188eu

BuildRequires: redhat-rpm-config, perl
ExclusiveArch: x86_64

# Sources.
Source0:  rtl%{kmod_name}-master.zip
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
%setup -q -n rtl%{kmod_name}-master
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
%{__make} KSRC=%{_usrsrc}/kernels/%{kversion}

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
* Sat Sep 07 2019 Philip J Perry <phil@elrepo.org> - 5.2.2.4-1.20190907git
- Updated to latest upstream git snapshot
- Rebuilt against RHEL 7.7 kernel

* Tue Apr 10 2018 Philip J Perry <phil@elrepo.org> - 4.1.4_6773.20130222-4
- Rebuilt against RHEL 7.5 kernel

* Sat Oct 14 2017 Philip J Perry <phil@elrepo.org> - 4.1.4_6773.20130222-3
- Updated to latest upstream git snapshot
- Rebuilt based on the RHEL 7.4 kernel-3.10.0-693.el7
  [http://elrepo.org/bugs/view.php?id=739]

* Fri Apr 07 2017 Alan Bartlett <ajb@elrepo.org> - 4.1.4_6773.20130222-2
- Rebuilt based on the RHEL 7.3 kernel-3.10.0-514.el7

* Tue Jul 29 2014 Alan Bartlett <ajb@elrepo.org> - 4.1.4_6773.20130222-1
- Initial el7 build of the kmod package.
