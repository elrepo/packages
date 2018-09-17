# Define the kmod package name here.
%define kmod_name nvidia

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-862.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 390.87
Release: 1%{?dist}
Group:   System Environment/Kernel
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
Source15: nvidia-provides.sh

NoSource: 0

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

# Define for nvidia-provides
%define __find_provides %{SOURCE15}

%description
This package provides the proprietary NVIDIA OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override %{kmod_name}-drm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_name}-modeset * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_name}-uvm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
sh %{SOURCE0} --extract-only --target nvidiapkg
%{__cp} -a nvidiapkg _kmod_build_

%build
export SYSSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/kernel
%{__make} %{?_smp_mflags} module
popd

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
pushd _kmod_build_/kernel
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-drm.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-modeset.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-uvm.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
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
* Mon Aug 27 2018 Philip J Perry <phil@elrepo.org> - 390.87-1
- Updated to version 390.87

* Tue Jul 17 2018 Philip J Perry <phil@elrepo.org> - 390.77-1
- Updated to version 390.77

* Wed Jun 06 2018 Philip J Perry <phil@elrepo.org> - 390.67-1
- Updated to version 390.67

* Fri May 18 2018 Philip J Perry <phil@elrepo.org> - 390.59-1
- Updated to version 390.59

* Tue Apr 10 2018 Philip J Perry <phil@elrepo.org> - 390.48-2
- Rebuilt against RHEL 7.5 kernel

* Fri Mar 30 2018 Philip J Perry <phil@elrepo.org> - 390.48-1
- Updated to version 390.48

* Fri Mar 16 2018 Philip J Perry <phil@elrepo.org> - 390.42-1
- Updated to version 390.42
- Built against latest kernel for retpoline support

* Tue Jan 30 2018 Philip J Perry <phil@elrepo.org> - 390.25-1
- Updated to version 390.25

* Fri Jan 05 2018 Philip J Perry <phil@elrepo.org> - 384.111-1
- Updated to version 384.111

* Fri Nov 03 2017 Philip J Perry <phil@elrepo.org> - 384.98-1
- Updated to version 384.98

* Sat Sep 23 2017 Philip J Perry <phil@elrepo.org> - 384.90-1
- Updated to version 384.90

* Sat Sep 02 2017 Akemi Yagi <toracat@elrepo.org> - 384.69-1
- Updated to version 384.69

* Thu Aug 31 2017 Akemi Yagi <toracat@elrepo.org> - 384.66-1
- Updated to version 384.66

* Tue Aug 01 2017 Philip J Perry <phil@elrepo.org> - 384.59-2
- Rebuilt against RHEL 7.4 kernel

* Tue Jul 25 2017 Philip J Perry <phil@elrepo.org> - 384.59-1
- Updated to version 384.59
- Reinstate support for GRID K520

* Wed May 10 2017 Philip J Perry <phil@elrepo.org> - 375.66-1
- Updated to version 375.66
- Blacklist GRID K1/K2/K340/K520 based devices no longer
  supported by the 375.xx driver
  [https://elrepo.org/bugs/view.php?id=724]
- Add provides for better compatibility with CUDA
  [http://elrepo.org/bugs/view.php?id=735]

* Fri Mar 03 2017 Philip J Perry <phil@elrepo.org> - 375.39-2
- Rebuilt against kernel-3.10.0-514.10.2.el7 for kABI breakage

* Wed Feb 22 2017 Philip J Perry <phil@elrepo.org> - 375.39-1
- Updated to version 375.39

* Thu Dec 15 2016 Philip J Perry <phil@elrepo.org> - 375.26-1
- Updated to version 375.26

* Sat Nov 19 2016 Philip J Perry <phil@elrepo.org> - 375.20-1
- Updated to version 375.20

* Thu Nov 03 2016 Philip J Perry <phil@elrepo.org> - 367.57-2
- Rebuilt against RHEL 7.3 kernel

* Tue Oct 11 2016 Philip J Perry <phil@elrepo.org> - 367.57-1
- Updated to version 367.57

* Sat Aug 27 2016 Philip J Perry <phil@elrepo.org> - 367.44-1
- Updated to version 367.44

* Sat Jul 16 2016 Philip J Perry <phil@elrepo.org> - 367.35-1
- Updated to version 367.35

* Tue Jun 14 2016 Philip J Perry <phil@elrepo.org> - 367.27-1
- Updated to version 367.27
- Adds nvidia-drm kernel module

* Wed May 25 2016 Philip J Perry <phil@elrepo.org> - 361.45.11-1
- Updated to version 361.45.11

* Thu Mar 31 2016 Philip J Perry <phil@elrepo.org> - 361.42-1
- Updated to version 361.42

* Tue Mar 01 2016 Philip J Perry <phil@elrepo.org> - 361.28-1
- Updated to version 361.28
- Adds nvidia-modeset kernel module

* Sun Jan 31 2016 Philip J Perry <phil@elrepo.org> - 352.79-1
- Updated to version 352.79

* Fri Nov 20 2015 Philip J Perry <phil@elrepo.org> - 352.63-1
- Updated to version 352.63
- Rebuilt against RHEL 7.2 kernel

* Sat Oct 17 2015 Philip J Perry <phil@elrepo.org> - 352.55-1
- Updated to version 352.55

* Sat Aug 29 2015 Philip J Perry <phil@elrepo.org> - 352.41-1
- Updated to version 352.41

* Sat Aug 01 2015 Philip J Perry <phil@elrepo.org> - 352.30-1
- Updated to version 352.30

* Fri Jul 03 2015 Philip J Perry <phil@elrepo.org> - 352.21-3
- Add blacklist() provides.
- Revert modalias() provides.

* Wed Jul 01 2015 Philip J Perry <phil@elrepo.org> - 352.21-2
- Add modalias() provides.

* Wed Jun 17 2015 Philip J Perry <phil@elrepo.org> - 352.21-1
- Updated to version 352.21

* Wed Apr 08 2015 Philip J Perry <phil@elrepo.org> - 346.59-1
- Updated to version 346.59

* Thu Mar 05 2015 Philip J Perry <phil@elrepo.org> - 346.47-2
- Rebuilt against RHEL 7.1 kernel

* Wed Feb 25 2015 Philip J Perry <phil@elrepo.org> - 346.47-1
- Updated to version 346.47

* Sat Jan 17 2015 Philip J Perry <phil@elrepo.org> - 346.35-1
- Updated to version 346.35
- Drops support of older G8x, G9x, and GT2xx GPUs
- Drops support for UVM on 32-bit architectures

* Fri Dec 12 2014 Philip J Perry <phil@elrepo.org> - 340.65-1
- Updated to version 340.65

* Thu Nov 06 2014 Philip J Perry <phil@elrepo.org> - 340.58-1
- Updated to version 340.58

* Sat Oct 04 2014 Philip J Perry <phil@elrepo.org> - 340.46-1
- Updated to version 340.46

* Sat Aug 16 2014 Philip J Perry <phil@elrepo.org> - 340.32-1
- Updated to version 340.32

* Wed Jul 09 2014 Philip J Perry <phil@elrepo.org> - 340.24-1
- Updated to version 340.24
- Enabled Secure Boot

* Sat Jul 05 2014 Philip J Perry <phil@elrepo.org> - 331.89-1
- Updated to version 331.89

* Tue Jun 10 2014 Philip J Perry <phil@elrepo.org> - 331.79-2
- Rebuilt for rhel-7.0 release

* Wed May 21 2014 Philip J Perry <phil@elrepo.org> - 331.79-1
- Initial el7 build of the nvidia kmod package.
