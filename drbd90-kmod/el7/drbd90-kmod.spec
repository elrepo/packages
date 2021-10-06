# Define the kmod package name here.
%define kmod_name drbd90
%define real_name drbd

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1160.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 9.1.4
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: Distributed Redundant Block Device driver for Linux
URL:     http://www.drbd.org/

BuildRequires: perl
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
# Source0:  http://oss.linbit.com/drbd/9.0/drbd-%{version}.tar.gz
Source0:  http://www.linbit.com/downloads/drbd/9.0/drbd-%{version}-1.tar.gz
Source10: kmodtool-%{kmod_name}-el7.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
DRBD is a distributed replicated block device. It mirrors a
block device over the network to another machine. Think of it
as networked raid 1. It is a building block for setting up
high availability (HA) clusters.

%prep
%setup -n %{real_name}-%{version}-1
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} %{?_smp_mflags} module KDIR=${KSRC} KVER=%{kversion}

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drbd/*.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
for file in ChangeLog COPYING README.md; do
    %{__install} -Dp -m0644 $file %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/$file
done

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
* Wed Oct 05 2021 Akemi Yagi <toracat@elrepo.org> - 9.1.4-1.el7_9
- Updated to 9.1.4-1
  [https://elrepo.org/bugs/view.php?id=1146]

* Fri Jul 30 2021 Philip J Perry <phil@elrepo.org> - 9.0.30-1.el7_9
- Updated to 9.0.30-1
  [https://elrepo.org/bugs/view.php?id=1124]

* Tue Sep 29 2020 Akemi Yagi <toracat@elrepo.org> - 9.0.22-3.el7_9
- Rebuilt against RHEL 7.9 kernel

* Sat Apr 04 2020 Akemi Yagi <toracat@elrepo.org> - 9.0.22-2.el7_8
- Updated to 9.0.22-2
- Rebuilt against RHEL 7.8 kernel

* Thu Oct 17 2019 Akemi Yagi <toracat@elrepo.org> - 9.0.20-1.el7_7
- Updated to 9.0.20
- Rebuild against RHEL 7.7 kernel

* Sat Nov 03 2018 Akemi Yagi <toracat@elrepo.org> - 9.0.16-1.el7_6
- Updated to 9.0.16
- Rebuild against RHEL 7.6 kernel

* Thu May 03 2018 Akemi Yagi <toracat@elrepo.org> - 9.0.14-1.el7_5
- Updated to 9.0.14

* Wed Apr 18 2018 Akemi Yagi <toracat@elrepo.org> - 9.0.13-1.el7_5
- Updated to 9.0.13
- Rebuild against RHEL 7.5 kernel

* Thu Sep 14 2017 Akemi Yagi <toracat@elrepo.org> - 9.0.9-1
- Updated to 9.0.9
- Built against EL7.4 kernel

* Fri Jun 30 2017 Philip J Perry <phil@elrepo.org> - 9.0.8-1
- Updated to 9.0.8

* Sat Jun 10 2017 Akemi Yagi <toracat@elrepo.org> - 9.0.7-1
- Updated to 9.0.7

* Wed Jun 24 2015 Hiroshi Fujishima <h-fujishima@sakura.ad.jp> - 9.0.0-1
- Initial el7 build of the kmod package.
