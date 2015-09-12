# Define the kmod package name here.
%define	 kmod_name nvidia-340xx

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-573.el6.%{_target_cpu}}

Name:	 %{kmod_name}-kmod
Version: 340.93
Release: 1%{?dist}
Group:	 System Environment/Kernel
License: Proprietary
Summary: NVIDIA OpenGL kernel driver module
URL:	 http://www.nvidia.com/

BuildRequires:	redhat-rpm-config
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i686 x86_64

# Sources.
Source0:  ftp://download.nvidia.com/XFree86/Linux-x86/%{version}/NVIDIA-Linux-x86-%{version}.run
Source1:  ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
Source10: kmodtool-%{kmod_name}-el6.sh

NoSource: 0
NoSource: 1

# Magic hidden here.
%define kmodtool sh %{SOURCE10}
%{expand:%(%{kmodtool} rpmtemplate %{kmod_name} %{kversion} "" 2>/dev/null)}

# Disable building of the debug package(s).
%define	debug_package %{nil}

%description
This package provides the proprietary NVIDIA OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override nvidia * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override nvidia-uvm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf

%ifarch i686
sh %{SOURCE0} --extract-only --target nvidiapkg
%endif

%ifarch x86_64
sh %{SOURCE1} --extract-only --target nvidiapkg
%endif

%{__cp} -a nvidiapkg _kmod_build_

%build
export SYSSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/kernel
%{__make} module
popd
pushd _kmod_build_/kernel/uvm
%{__make} module
popd

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
ksrc=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/kernel
%{__make} -C "${ksrc}" modules_install M=$PWD
popd
pushd _kmod_build_/kernel/uvm
%{__make} -C "${ksrc}" modules_install M=$PWD
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Sat Sep 12 2015 Philip J Perry <phil@elrepo.org> - 340.93-1.el6.elrepo
- Updated to version 340.93

* Sat Aug 01 2015 Philip J Perry <phil@elrepo.org> - 340.76-2.el6.elrepo
- Rebuild against RHEL-6.7 kernel
  [http://elrepo.org/bugs/view.php?id=583]

* Thu Feb 05 2015 Philip J Perry <phil@elrepo.org> - 340.76-1.el6.elrepo
- Updated to version 340.76

* Tue Dec 16 2014 Philip J Perry <phil@elrepo.org> - 340.65-1.el6.elrepo
- Updated to version 340.65

* Fri Sep 26 2014 Philip J Perry <phil@elrepo.org> - 340.32-1.el6.elrepo
- Fork to legacy release nvidia-340xx
- Trimmed changelog

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

