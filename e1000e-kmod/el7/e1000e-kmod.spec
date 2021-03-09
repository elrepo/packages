# Define the kmod package name here.
%define kmod_name e1000e

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1160.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 3.8.4
Release: 3.el7_9.elrepo
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

# Patches.
# Patch0: ELRepo-%{kmod_name}-%{version}.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s) for the
Intel® 82563/6/7, 82571/2/3/4/7/8/9 and 82583 PCI-E based Ethernet NICs.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}
# %patch0 -p1
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
* Mon Feb 01 2021 Akemi Yagi <toracat@elrepo.org> - 3.8.4-3.el7_9
- Built against kernel-3.10.0-1160.el7
  [https://elrepo.org/bugs/view.php?id=1071]

* Wed Sep 16 2020 Akemi Yagi <toracat@elrepo.org> - 3.8.4-2.el7_8
- Built against kernel-3.10.0-1127.19.1.el7
  [https://elrepo.org/bugs/view.php?id=1040]

* Tue Sep 01 2020 Akemi Yagi <toracat@elrepo.org> - 3.8.4-1.el7_8
- Updated to version 3.8.4
- Built against RHEL 7.8 kernel

* Wed Apr 11 2018 Akemi Yagi <toracat@elrepo.org> - 3.4.0.2-1.el7_5
- Rebuilt against RHEL 7.5 kernel

* Sat Mar 24 2018 Akemi Yagi <toracat@elrepo.org> - 3.4.0.2-1
- Updated version to 3.4.0.2
- Built against retpoline-aware kernel 3.10.0-693.21.1
 
* Sat Aug 26 2017 Akemi Yagi <toracat@elrepo.org> - 3.3.5.10-1
- Updated version to 3.3.5.10
- Built against RHEL 7.4 kernel (3.10.0-693)

* Wed Jul 26 2017 Akemi Yagi <toracat@elrepo.org> - 3.3.5-1
- Updated version to 3.3.5

* Thu Dec 29 2016 Alan Bartlett <ajb@elrepo.org> - 3.3.4-2
- Rebuilt against the RHEL 7.3 kernel

* Fri Jun 03 2016 Akemi Yagi <toracat@elrepo.org> - 3.3.4-1
- Updated version to 3.3.4

* Wed Apr 13 2016 Akemi Yagi <toracat@elrepo.org> - 3.3.3-1
- Updated version to 3.3.3
- Built against RHEL 7.2 kernel

* Thu Mar 05 2015 Philip J Perry <phil@elrepo.org> - 3.1.0.2-2
- Rebuilt against RHEL 7.1 kernel

* Sun Aug 03 2014 Alan Bartlett <ajb@elrepo.org> - 3.1.0.2-1
- Updated version to 3.1.0.2

* Fri Jun 13 2014 Alan Bartlett <ajb@elrepo.org> - 3.0.4-1
- Initial el7 build of the kmod package.
