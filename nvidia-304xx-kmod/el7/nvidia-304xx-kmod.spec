# Define the kmod package name here.
%define	 kmod_name nvidia-304xx

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-957.el7.%{_target_cpu}}

Name:	 %{kmod_name}-kmod
Version: 304.135
Release: 6%{?dist}
Group:	 System Environment/Kernel
License: Proprietary
Summary: NVIDIA OpenGL kernel driver module
URL:	 http://www.nvidia.com/

BuildRequires: perl
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
Source0:  ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
Source1:  blacklist-nouveau.conf
Source10: kmodtool-%{kmod_name}-el7.sh

Patch0:   legacy304.patch
Patch1:   nvidia-304.135-el7.5-get-user-pages.patch
Patch2:   nvidia-304.135-el7.5-nv_drm_legacy.patch

NoSource: 0

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the proprietary NVIDIA OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override nvidia * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
sh %{SOURCE0} --extract-only --target nvidiapkg

%patch0 -p1
%patch1 -p1
%patch2 -p1

%{__cp} -a nvidiapkg _kmod_build_

%build
export SYSSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/kernel
%{__make} module
popd

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
pushd _kmod_build_/kernel
%{__install} nvidia.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} %{SOURCE1} %{buildroot}%{_prefix}/lib/modprobe.d/blacklist-nouveau.conf

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
* Tue Oct 30 2018 Philip J Perry <phil@elrepo.org> - 304.135-6
- Rebuilt against RHEL 7.6 kernel

* Tue Apr 10 2018 Philip J Perry <phil@elrepo.org> - 304.135-5
- Rebuilt against RHEL 7.5 kernel
- Fix get_user_pages and get_user_pages_remote compile errors
- Fix drm_legacy_pci functions compile errors

* Sat Sep 16 2017 Philip J Perry <phil@elrepo.org> - 304.135-4
- Patch to fix compilation issue applied
  [http://elrepo.org/bugs/view.php?id=780]

* Sat Aug 05 2017 Philip J Perry <phil@elrepo.org> - 304.135-3
- Rebuilt against RHEL 7.4 kernel

* Fri Mar 03 2017 Philip J Perry <phil@elrepo.org> - 304.135-2
- Rebuilt against kernel-3.10.0-514.10.2.el7 for kABI breakage

* Sat Feb 25 2017 Philip J Perry <phil@elrepo.org> - 304.135-1
- Updated to version 304.135

* Sat Dec 17 2016 Philip J Perry <phil@elrepo.org> - 304.134-1
- Updated to version 304.134

* Sat Dec 10 2016 Philip J Perry <phil@elrepo.org> - 304.131-2
- Rebuilt against RHEL 7.3 kernel

* Fri Nov 20 2015 Philip J Perry <phil@elrepo.org> - 304.131-1
- Updated to version 304.131
- Rebuilt against RHEL 7.2 kernel

* Thu Mar 05 2015 Philip J Perry <phil@elrepo.org> - 304.125-2
- Rebuilt against RHEL 7.1 kernel

* Fri Dec 19 2014 Philip J Perry <phil@elrepo.org> - 304.125-1
- Updated to version 304.125

* Fri Jul 18 2014 Philip J Perry <phil@elrepo.org> - 304.123-1
- Port 304.xx legacy driver to RHEL7.
- Updated to version 304.123
