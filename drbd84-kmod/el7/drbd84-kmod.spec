# Define the kmod package name here.
%define kmod_name drbd84
%define real_name drbd

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-862.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 8.4.11
%define  original_release 1
Release: %{original_release}%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: Distributed Redundant Block Device driver for Linux
URL:     http://www.drbd.org/

BuildRequires: perl
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
Source0:  http://oss.linbit.com/drbd/8.4/drbd-%{version}-%{original_release}.tar.gz
Source10: kmodtool-%{kmod_name}-el7.sh

#Patches

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
%setup -n %{real_name}-%{version}-%{original_release}
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
* Thu Apr 26 2018 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1
- Built against RHEL 7.5 kernel 3.10.0-862.el7
- Updated to version 8.4.11

* Fri Sep 15 2017 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1_2
- Built against RHEL 7.4 kernel 3.10.0-693.el7
- Add patch from git [elrepo bug 781]

* Mon Jun 12 2017 Akemi Yagi <toracat@elrepo.org> - 8.4.10-1
- Updated to version 8.4.10.

* Sat Dec  3 2016 Akemi Yagi <toracat@elrepo.org> - 8.4.9-1
- Updated to version 8.4.9-1.

* Tue Nov  8 2016 Akemi Yagi <toracat@elrepo.org> - 8.4.8-1_2
- Rebuilt against kernel 3.10.0-514.

* Wed Oct  5 2016 Hiroshi Fujishima <h-fujishima@sakura.ad.jp> - 8.4.8-1_1
- Updated to version 8.4.8-1.

* Mon Jan  4 2016 Hiroshi Fujishima <h-fujishima@sakura.ad.jp> - 8.4.7-1_1
- Updated to version 8.4.7-1.

* Thu Mar 05 2015 Philip J Perry <phil@elrepo.org> - 8.4.5-2
- Rebuilt against RHEL 7.1 kernel

* Sat Jul 19 2014 Philip J Perry <phil@elrepo.org> - 8.4.5-1
- Initial el7 build of the kmod package.
