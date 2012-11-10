# Define the kmod package name here.
%define	 kmod_name nvidia

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-279.el6.%{_target_cpu}}

Name:	 %{kmod_name}-kmod
Version: 304.60
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
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

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

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
pushd _kmod_build_/kernel
ksrc=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${ksrc}" modules_install M=$PWD
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Oct 19 2012 Philip J Perry <phil@elrepo.org> - 304-60-1.el6.elrepo
- Updated to version 304.60

* Fri Sep 28 2012 Philip J Perry <phil@elrepo.org> - 304-51-1.el6.elrepo
- Updated to version 304.51

* Tue Aug 28 2012 Philip J Perry <phil@elrepo.org> - 304-43-1.el6.elrepo
- Updated to version 304.43

* Tue Aug 14 2012 Philip J Perry <phil@elrepo.org> - 304-37-1.el6.elrepo
- Updated to version 304.37
- Built against kernel-2.6.32-279.el6

* Wed Aug 08 2012 Philip J Perry <phil@elrepo.org> - 295.71-1.el5.elrepo
- Updated to version 295.71
- Fixes http://permalink.gmane.org/gmane.comp.security.full-disclosure/86747

* Tue Jun 19 2012 Philip J Perry <phil@elrepo.org> - 302-17-1.el6.elrepo
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
