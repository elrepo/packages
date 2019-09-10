# Define the kmod package name here.
%define kmod_name ixgbe

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1062.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 5.6.3
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.intel.com/

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
%{__gzip} %{kmod_name}.7
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
pushd src >/dev/null
%{__make} KSRC=%{_usrsrc}/kernels/%{kversion}
popd >/dev/null

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} pci.updates %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -d %{buildroot}%{_mandir}/man7/
%{__install} %{kmod_name}.7.gz %{buildroot}%{_mandir}/man7/

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
* Mon Sep 02 2019 Akemi Yagi <toracat@elrepo.org> - 5.6.3-1
- Updated to version 5.6.3
- Built against RHEL 7.7 kernel

* Thu Mar 28 2019 Akemi Yagi <toracat@elrepo.org> - 5.5.5-1
- Updated to version 5.5.5
- Built against RHEL 7.6 kernel

* Mon Sep 18 2017 Akemi Yagi <toracat@elrepo.org> - 5.2.1-3
- Updated to version 5.2.3

* Tue Aug 15 2017 Akemi Yagi <toracat@elrepo.org> - 5.2.1-1
- Updated to version 5.2.1
- Built against RHEL 7.4 kernel

* Thu Dec 22 2016 Alan Bartlett <ajb@elrepo.org> - 4.5.4-1
- Updated to version 4.5.4

* Mon May 11 2015 Alan Bartlett <ajb@elrepo.org> - 3.23.2.1-1
- Updated to version 3.23.2.1

* Thu Mar 05 2015 Philip J Perry <phil@elrepo.org> - 3.23.2-2
- Rebuilt against RHEL 7.1 kernel

* Mon Dec 29 2014 Alan Bartlett <ajb@elrepo.org> - 3.23.2-1
- Updated to version 3.23.2

* Sat Oct 04 2014 Alan Bartlett <ajb@elrepo.org> - 3.22.3-1
- Updated to version 3.22.3

* Fri Jun 13 2014 Alan Bartlett <ajb@elrepo.org> - 3.21.2-1
- Initial el7 build of the kmod package.
