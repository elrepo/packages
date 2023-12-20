# Define the kmod package name here.
%define kmod_name r8125

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1160.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 9.011.01
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.realtek.com/en/

BuildRequires: redhat-rpm-config, perl
ExclusiveArch: x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.bz2
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh
Source20: ELRepo-Makefile-%{kmod_name}

# Patches.
Patch0: ELRepo-r8125-%{version}.patch

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
%patch0 -p1
%{__rm} -f src/Makefile*
%{__cp} -a %{SOURCE20} src/Makefile
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD/src

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
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
* Wed May 31 2023 Akemi Yagi <toracat@elrepo.org> - 9.011.01-1
- Update to version 9.011.01
  [https://elrepo.org/bugs/view.php?id=1356]

* Wed Jan 04 2023 Philip J Perry <phil@elrepo.org> - 9.011.00-1
- Update to version 9.011.00
  [https://elrepo.org/bugs/view.php?id=1305]
- Enable Double VLAN

* Fri Oct 14 2022 Philip J Perry <phil@elrepo.org> - 9.009.02-1
- Update to version 9.009.02
  [https://elrepo.org/bugs/view.php?id=1279]

* Tue Nov 23 2021 Philip J Perry <phil@elrepo.org> - 9.007.01-1
- Update to version 9.007.01
  [https://elrepo.org/bugs/view.php?id=1165]

* Mon Nov 08 2021 Philip J Perry <phil@elrepo.org> - 9.006.04-1
- Update to version 9.006.04
  [https://elrepo.org/bugs/view.php?id=1157]

* Thu Feb 25 2021 Philip J Perry <phil@elrepo.org> - 9.005.01-1
- Update to version 9.005.01
  [https://elrepo.org/bugs/view.php?id=1078]
- Rebuilt against RHEL7.9 kernel
  
* Tue Aug 11 2020 Philip J Perry <phil@elrepo.org> - 9.003.05-1
- Initial el7 build of the kmod package.
