# Define the kmod package name here.
%define kmod_name tpe
%define src_name tpe-lkm

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-862.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 2.0.3
Release: 6.20170731git%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     https://github.com/cormander/tpe-lkm

BuildRequires: perl
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
# http://sourceforge.net/projects/tpe-lkm/files/latest/download
Source0: %{src_name}-%{version}.tar.gz
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
%setup -q -n %{src_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -p conf/tpe.modprobe.conf %{buildroot}%{_sysconfdir}/modprobe.d/tpe.conf
%{__install} -d %{buildroot}%{_sysconfdir}/sysconfig/
%{__install} -p conf/tpe-*-whitelist %{buildroot}%{_sysconfdir}/sysconfig/
%{__install} -d %{buildroot}%{_sysconfdir}/sysconfig/modules/
%{__install} -p conf/tpe.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/modules/tpe.modules
%{__install} -d %{buildroot}%{_sysconfdir}/sysctl.d/
%{__install} -p conf/tpe.sysctl %{buildroot}%{_sysconfdir}/sysctl.d/tpe.conf
%{__install} -d %{buildroot}%{_sbindir}/
%{__install} -p scripts/tpe-setfattr-whitelist.sh %{buildroot}%{_sbindir}/tpe-setfattr-whitelist.sh
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -p {FAQ,GPL,INSTALL,LICENSE,README} \
    %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

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
* Tue Apr 10 2018 Philip J Perry <phil@elrepo.org> - 2.0.3-6.20170731git
- Rebuilt against RHEL 7.5 kernel

* Sun Mar 18 2018 Philip J Perry <phil@elrepo.org> - 2.0.3-5.20170731git
- Rebuilt against latest kernel for retpoline support

* Wed Aug 02 2017 Philip J Perry <phil@elrepo.org> - 2.0.3-4.20170731git
- Update tpe-mmap-whitelist for KDE apps
- Rebuilt against RHEL 7.4 kernel

* Sun May 07 2017 Philip J Perry <phil@elrepo.org> - 2.0.3-3.20170507git
- Add all tpe whitelist files

* Sun May 07 2017 Philip J Perry <phil@elrepo.org> - 2.0.3-2.20170507git
- Fix soften regression with tpe.extras
- Add requires for setfattr

* Wed May 03 2017 Philip J Perry <phil@elrepo.org> - 2.0.3-1
- Update to 2.0.3

* Tue Apr 25 2017 Philip J Perry <phil@elrepo.org> - 2.0.2-2.20170425git
- Fix race condition on module insert

* Sun Apr 23 2017 Philip J Perry <phil@elrepo.org> - 2.0.2-1
- Update to 2.0.2

* Thu Apr 06 2017 Philip J Perry <phil@elrepo.org> - 2.0.1-3.20170406git
- Whitelist more programs
- Update %%triggerin list

* Tue Apr 04 2017 Philip J Perry <phil@elrepo.org> - 2.0.1-2.20170404git
- Add tpe.xattr_soften to sysctl conf file

* Mon Apr 03 2017 Philip J Perry <phil@elrepo.org> - 2.0.1-1.20170403git
- Update to 2.0.1

* Sat Apr 01 2017 Philip J Perry <phil@elrepo.org> - 2.0.0-5.20170401git
- Quiet down the gnome-helper
- Silence tpe-setfattr-whitelist script

* Sat Apr 01 2017 Philip J Perry <phil@elrepo.org> - 2.0.0-4.20170401git
- Upstream fixes to setfattr.
- Added %%triggerin script for packages on the whitelist

* Fri Mar 31 2017 Philip J Perry <phil@elrepo.org> - 2.0.0-3.20170331git
- Add security.tpe extended attribute support to soften checks

* Mon Mar 27 2017 Philip J Perry <phil@elrepo.org> - 2.0.0-2.20170327git
- More upstream updates, adds tpe.extras
- Add requires for kernel >= 3.10.0-327.el7

* Sat Mar 25 2017 Philip J Perry <phil@elrepo.org> - 2.0.0-1.20170325git
- Update to 2.0.0
- Complete rewrite to use kernel ftrace framework

* Mon Sep 01 2014 Philip J Perry <phil@elrepo.org> - 1.1.0-1
- Update to 1.1.0

* Sat Aug 23 2014 Philip J Perry <phil@elrepo.org> - 1.0.3-991.20140823git
- Update to latest git snapshot as a beta for 1.0.4 release.

* Thu Aug 21 2014 Philip J Perry <phil@elrepo.org> - 1.0.3-990.git20140821
- Initial el7 port of the kmod package.
- Update to latest git snapshot as a beta for 1.0.4 release.
