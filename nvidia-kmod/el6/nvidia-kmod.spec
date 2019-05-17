# Define the kmod package name here.
%define	 kmod_name nvidia

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-754.el6.%{_target_cpu}}

Name:	 %{kmod_name}-kmod
Version: 430.14
Release: 1%{?dist}
Group:	 System Environment/Kernel
License: Proprietary
Summary: NVIDIA OpenGL kernel driver module
URL:	 http://www.nvidia.com/

BuildRequires:	redhat-rpm-config
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	x86_64

# Sources.
Source0:  ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
Source10: kmodtool-%{kmod_name}-el6.sh
Source15: nvidia-provides.sh

NoSource: 0

# Magic hidden here.
%define kmodtool sh %{SOURCE10}
%{expand:%(%{kmodtool} rpmtemplate %{kmod_name} %{kversion} "" 2>/dev/null)}

# Disable building of the debug package(s).
%define	debug_package %{nil}

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
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
ksrc=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/kernel
%{__make} -C "${ksrc}" modules_install M=$PWD
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Tue May 14 2019 Philip J Perry <phil@elrepo.org> - 430.14-1
- Updated to version 430.14

* Tue May 07 2019 Philip J Perry <phil@elrepo.org> - 418.74-1
- Updated to version 418.74

* Thu Mar 21 2019 Philip J Perry <phil@elrepo.org> - 418.56-1
- Updated to version 418.56

* Sat Mar 02 2019 Philip J Perry <phil@elrepo.org> - 418.43-1
- Updated to version 418.43

* Sat Jan 05 2019 Philip J Perry <phil@elrepo.org> - 410.93-1
- Updated to version 410.93

* Thu Nov 15 2018 Philip J Perry <phil@elrepo.org> - 410.78-1
- Updated to version 410.78

* Thu Oct 25 2018 Philip J Perry <phil@elrepo.org> - 410.73-1
- Updated to version 410.73

* Tue Oct 16 2018 Philip J Perry <phil@elrepo.org> - 410.66-1
- Updated to version 410.66

* Sat Sep 22 2018 Philip J Perry <phil@elrepo.org> - 410.57-1
- Updated to version 410.57 beta driver
- Remove 32-bit OS support

* Mon Sep 17 2018 Philip J Perry <phil@elrepo.org> - 396.54-1
- Updated to version 396.54

* Mon Aug 27 2018 Philip J Perry <phil@elrepo.org> - 390.87-1
- Updated to version 390.87

* Tue Jul 17 2018 Philip J Perry <phil@elrepo.org> - 390.77-1
- Updated to version 390.77
- Built against RHEL-6.10 kernel

* Wed Jun 06 2018 Philip J Perry <phil@elrepo.org> - 390.67-1
- Updated to version 390.67

* Fri May 18 2018 Philip J Perry <phil@elrepo.org> - 390.59-1
- Updated to version 390.59

* Fri Mar 30 2018 Philip J Perry <phil@elrepo.org> - 390.48-1
- Updated to version 390.48

* Fri Mar 16 2018 Philip J Perry <phil@elrepo.org> - 390.42-1
- Updated to version 390.42
- Built against latest kernel for retpoline supported

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

* Wed Feb 22 2017 Philip J Perry <phil@elrepo.org> - 375.39-1
- Updated to version 375.39

* Thu Dec 15 2016 Philip J Perry <phil@elrepo.org> - 375.26-1
- Updated to version 375.26

* Sat Nov 19 2016 Philip J Perry <phil@elrepo.org> - 375.20-1
- Updated to version 375.20

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

* Sat Oct 17 2015 Philip J Perry <phil@elrepo.org> - 352.55-1
- Updated to version 352.55

* Sat Aug 29 2015 Philip J Perry <phil@elrepo.org> - 352.41-1
- Updated to version 352.41

* Sat Aug 01 2015 Philip J Perry <phil@elrepo.org> - 352.30-1
- Updated to version 352.30
- Built against RHEL-6.7 kernel

* Fri Jul 03 2015 Philip J Perry <phil@elrepo.org> - 352.21-3
- Add blacklist() provides.
- Revert modalias() provides.

* Wed Jul 01 2015 Philip J Perry <phil@elrepo.org> - 352.21-2
- Add modalias() provides.

* Wed Jun 17 2015 Philip J Perry <phil@elrepo.org> - 352.21-1
- Updated to version 352.21

* Wed Apr 08 2015 Philip J Perry <phil@elrepo.org> - 346.59-1
- Updated to version 346.59

* Wed Feb 25 2015 Philip J Perry <phil@elrepo.org> - 346.47-1
- Updated to version 346.47

* Sat Jan 17 2015 Philip J Perry <phil@elrepo.org> - 346.35-1
- Updated to version 346.35
- Drops support of older G8x, G9x, and GT2xx GPUs
- Drops support for UVM on 32-bit architectures

* Fri Dec 12 2014 Philip J Perry <phil@elrepo.org> - 340.65-1.el6.elrepo
- Updated to version 340.65

* Thu Nov 06 2014 Philip J Perry <phil@elrepo.org> - 340.58-1.el6.elrepo
- Updated to version 340.58

* Sat Oct 04 2014 Philip J Perry <phil@elrepo.org> - 340.46-1.el6.elrepo
- Updated to version 340.46

* Sat Aug 16 2014 Philip J Perry <phil@elrepo.org> - 340.32-1.el6.elrepo
- Updated to version 340.32

* Wed Jul 09 2014 Philip J Perry <phil@elrepo.org> - 340.24-1.el6.elrepo
- Updated to version 340.24

* Sat Jul 05 2014 Philip J Perry <phil@elrepo.org> - 331.89-1.el6.elrepo
- Updated to version 331.89

* Wed May 21 2014 Philip J Perry <phil@elrepo.org> - 331.79-1.el6.elrepo
- Updated to version 331.79

* Sat May 03 2014 Philip J Perry <phil@elrepo.org> - 331.67-3.el6.elrepo
- Add nvidia-modprobe

* Fri May 02 2014 Philip J Perry <phil@elrepo.org> - 331.67-2.el6.elrepo
- Build the nvidia-uvm module required for CUDA

* Wed Apr 09 2014 Philip J Perry <phil@elrepo.org> - 331.67-1.el6.elrepo
- Updated to version 331.67

* Wed Feb 19 2014 Philip J Perry <phil@elrepo.org> - 331.49-1.el6.elrepo
- Updated to version 331.49

* Sat Jan 18 2014 Philip J Perry <phil@elrepo.org> - 331.38-1.el6.elrepo
- Updated to version 331.38

* Fri Nov 08 2013 Philip J Perry <phil@elrepo.org> - 331.20-1.el6.elrepo
- Updated to version 331.20

* Mon Aug 05 2013 Philip J Perry <phil@elrepo.org> - 325.15-1.el6.elrepo
- Updated to version 325.15

* Sun Jun 30 2013 Philip J Perry <phil@elrepo.org> - 319.32-1.el6.elrepo
- Updated to version 319.32

* Fri May 24 2013 Philip J Perry <phil@elrepo.org> - 319.23-1.el6.elrepo
- Updated to version 319.23

* Thu May 09 2013 Philip J Perry <phil@elrepo.org> - 319.17-1.el6.elrepo
- Updated to version 319.17

* Thu Apr 04 2013 Philip J Perry <phil@elrepo.org> - 310.44-1.el6.elrepo
- Updated to version 310.44

* Sat Mar 09 2013 Philip J Perry <phil@elrepo.org> - 310.40-1.el6.elrepo
- Updated to version 310.40

* Wed Jan 23 2013 Philip J Perry <phil@elrepo.org> - 310.32-1.el6.elrepo
- Updated to version 310.32

* Tue Nov 20 2012 Philip J Perry <phil@elrepo.org> - 310.19-2.el6.elrepo
- Fix broken SONAME dependency chain

* Mon Nov 19 2012 Philip J Perry <phil@elrepo.org> - 310.19-1.el6.elrepo
- Updated to version 310.19
- Drops support for older 6xxx and 7xxx series cards

* Sat Nov 10 2012 Philip J Perry <phil@elrepo.org> - 304.64-1.el6.elrepo
- Updated to version 304.64

* Fri Oct 19 2012 Philip J Perry <phil@elrepo.org> - 304.60-1.el6.elrepo
- Updated to version 304.60

* Fri Sep 28 2012 Philip J Perry <phil@elrepo.org> - 304.51-1.el6.elrepo
- Updated to version 304.51

* Tue Aug 28 2012 Philip J Perry <phil@elrepo.org> - 304.43-1.el6.elrepo
- Updated to version 304.43

* Tue Aug 14 2012 Philip J Perry <phil@elrepo.org> - 304.37-1.el6.elrepo
- Updated to version 304.37
- Built against kernel-2.6.32-279.el6

* Wed Aug 08 2012 Philip J Perry <phil@elrepo.org> - 295.71-1.el5.elrepo
- Updated to version 295.71
- Fixes http://permalink.gmane.org/gmane.comp.security.full-disclosure/86747

* Tue Jun 19 2012 Philip J Perry <phil@elrepo.org> - 302.17-1.el6.elrepo
- Updated to version 302.17

* Sat Jun 16 2012 Philip J Perry <phil@elrepo.org> - 295.59-1.el6.elrepo
- Updated to version 295.59

* Thu May 17 2012 Philip J Perry <phil@elrepo.org> - 295.53-1.el6.elrepo
- Updated to version 295.53

* Fri May 04 2012 Philip J Perry <phil@elrepo.org> - 295.49-1.el6.elrepo
- Updated to version 295.49

* Wed Apr 11 2012 Philip J Perry <phil@elrepo.org> - 295.40-1.el6.elrepo
- Updated to version 295.40
- Fixes CVE-2012-0946

* Fri Mar 23 2012 Philip J Perry <phil@elrepo.org> - 295.33-1.el6.elrepo
- Updated to version 295.33
- Build against RHEL-6.2 kernel to avoid alt_instr fixups.
  [http://elrepo.org/bugs/view.php?id=244]

* Mon Feb 13 2012 Philip J Perry <phil@elrepo.org> - 295.20-1.el6.elrepo
- Updated to version 295.20

* Wed Nov 23 2011 Philip J Perry <phil@elrepo.org> - 290.10-1.el6.elrepo
- Updated to version 290.10

* Fri Oct 07 2011 Philip J Perry <phil@elrepo.org> - 285.05.09-1.el6.elrepo
- Updated to version 285.05.09

* Tue Aug 02 2011 Philip J Perry <phil@elrepo.org> - 280.13-1.el6.elrepo
- Updated to version 280.13

* Fri Jul 22 2011 Philip J Perry <phil@elrepo.org> - 275.21-1.el6.elrepo
- Updated to version 275.21

* Fri Jul 15 2011 Philip J Perry <phil@elrepo.org> - 275.19-1.el6.elrepo
- Updated to version 275.19

* Fri Jun 17 2011 Philip J Perry <phil@elrepo.org> - 275.09.07-1.el6.elrepo
- Updated to version 275.09.07

* Sat Apr 16 2011 Philip J Perry <phil@elrepo.org> - 270.41.03-1.el6.elrepo
- Updated to version 270.41.03 for release

* Fri Mar 25 2011 Philip J Perry <phil@elrepo.org>
- Updated to version 270.30 beta

* Wed Mar 09 2011 Philip J Perry <phil@elrepo.org> - 260.19.44-1.el6.elrepo
- Updated to version 260.19.44

* Fri Jan 21 2011 Philip J Perry <phil@elrepo.org> - 260.19.36-1.el6.elrepo
- Updated to version 260.19.36

* Fri Dec 17 2010 Philip J Perry <phil@elrepo.org> - 260.19.29-1.el6.elrepo
- Updated to version 260.19.29

* Sun Nov 28 2010 Philip J Perry <phil@elrepo.org> - 260.19.21-1.el6.elrepo
- Rebuilt for release.

* Sun Nov 28 2010 Philip J Perry <phil@elrepo.org> - 260.19.21-0.4.el6.elrepo
- Rebuilt for testing release.

* Sun Nov 21 2010 Philip J Perry <phil@elrepo.org> - 260.19.21-0.3.el6.elrepo
- Rebuilt for testing release.

* Sun Nov 21 2010 Philip J Perry <phil@elrepo.org> - 260.19.21-0.2.el6.elrepo
- Fix udev device creation.

* Sat Nov 20 2010 Philip J Perry <phil@elrepo.org> - 260.19.21-0.1.el6.elrepo
- Initial build of the kmod package for RHEL6 GA release.

* Fri Apr 30 2010 Philip J Perry <phil@elrepo.org> - - 195.36.24-0.1.el6.elrepo
- Initial build for RHEL6beta1
