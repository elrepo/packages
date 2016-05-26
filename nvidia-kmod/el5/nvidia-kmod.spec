# Define the kmod package name here.
%define	 kmod_name nvidia

# If kversion isn't defined on the rpmbuild line, define it here.
# kABI compatible with kernel 2.6.18-194.el5 upwards
%{!?kversion: %define kversion 2.6.18-398.el5}

Name:    %{kmod_name}-kmod
Version: 361.45.11
Release: 1%{?dist}
Group:   System Environment/Kernel
License: Proprietary
Summary: NVIDIA OpenGL kernel driver module
URL:     http://www.nvidia.com/

BuildRequires: redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  ftp://download.nvidia.com/XFree86/Linux-x86/%{version}/NVIDIA-Linux-x86-%{version}.run
Source1:  ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run

Source10: kmodtool-%{kmod_name}-el5.sh
Source15: nvidia-provides.sh

NoSource: 0
NoSource: 1

# Define the variants for each architecture.
%define basevar ""
%ifarch i686
%define paevar PAE
%endif

# If kvariants isn't defined on the rpmbuild line, build all variants for this architecture.
%{!?kvariants: %define kvariants %{?basevar} %{?paevar}}

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants})}

# Disable the building of the debug package(s).
%define	debug_package %{nil}

# Define the filter.
%define __find_requires sh %{_builddir}/%{buildsubdir}/filter-requires.sh
# Define for nvidia-provides
%define __find_provides %{SOURCE15}

%description
This package provides the proprietary NVIDIA OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override %{kmod_name}-modeset * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
%ifarch x86_64
echo "override %{kmod_name}-uvm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
%endif

%ifarch i686
sh %{SOURCE0} --extract-only --target nvidiapkg
%endif

%ifarch x86_64
sh %{SOURCE1} --extract-only --target nvidiapkg
%endif

for kvariant in %{kvariants} ; do
    %{__cp} -a nvidiapkg _kmod_build_$kvariant
done

%build
for kvariant in %{kvariants} ; do
    export SYSSRC=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant/kernel
    %{__make} module
    popd
done

%install
%{__rm} -rf %{buildroot}
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant/kernel
    %{__make} -C "${ksrc}" modules_install M=$PWD
    popd
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -p -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/

%clean
%{__rm} -rf %{buildroot}

%changelog
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

* Fri Dec 12 2014 Philip J Perry <phil@elrepo.org> - 340.65-1.el5.elrepo
- Updated to version 340.65

* Thu Nov 06 2014 Philip J Perry <phil@elrepo.org> - 340.58-1.el5.elrepo
- Updated to version 340.58

* Sat Oct 04 2014 Philip J Perry <phil@elrepo.org> - 340.46-1.el5.elrepo
- Updated to version 340.46

* Sat Aug 16 2014 Philip J Perry <phil@elrepo.org> - 340.32-1.el5.elrepo
- Updated to version 340.32

* Wed Jul 09 2014 Philip J Perry <phil@elrepo.org> - 340.24-1.el5.elrepo
- Updated to version 340.24

* Sat Jul 05 2014 Philip J Perry <phil@elrepo.org> - 331.89-1.el5.elrepo
- Updated to version 331.89

* Wed May 21 2014 Philip J Perry <phil@elrepo.org> - 331.79-1.el5.elrepo
- Updated to version 331.79

* Sat May 03 2014 Philip J Perry <phil@elrepo.org> - 331.67-3.el5.elrepo
- Add nvidia-modprobe

* Fri May 02 2014 Philip J Perry <phil@elrepo.org> - 331.67-2.el5.elrepo
- Build the nvidia-uvm module required for CUDA

* Wed Apr 09 2014 Philip J Perry <phil@elrepo.org> - 331.67-1.el5.elrepo
- Updated to version 331.67

* Wed Feb 19 2014 Philip J Perry <phil@elrepo.org> - 331.49-1.el5.elrepo
- Updated to version 331.49

* Sat Jan 18 2014 Philip J Perry <phil@elrepo.org> - 331.38-1.el5.elrepo
- Updated to version 331.38

* Fri Nov 08 2013 Philip J Perry <phil@elrepo.org> - 331.20-1.el5.elrepo
- Updated to version 331.20

* Mon Aug 05 2013 Philip J Perry <phil@elrepo.org> - 325.15-1.el5.elrepo
- Updated to version 325.15

* Sun Jun 30 2013 Philip J Perry <phil@elrepo.org> - 319.32-1.el5.elrepo
- Updated to version 319.32

* Fri May 24 2013 Philip J Perry <phil@elrepo.org> - 319.23-1.el5.elrepo
- Updated to version 319.23

* Thu May 09 2013 Philip J Perry <phil@elrepo.org> - 319.17-1.el5.elrepo
- Updated to version 319.17

* Thu Apr 04 2013 Philip J Perry <phil@elrepo.org> - 310.44-1.el5.elrepo
- Updated to version 310.44

* Sat Mar 09 2013 Philip J Perry <phil@elrepo.org> - 310.40-1.el5.elrepo
- Updated to version 310.40

* Wed Jan 23 2013 Philip J Perry <phil@elrepo.org> - 310.32-1.el5.elrepo
- Updated to version 310.32

* Tue Nov 20 2012 Philip J Perry <phil@elrepo.org> - 310.19-2.el5.elrepo
- Fix broken SONAME dependency chain

* Mon Nov 19 2012 Philip J Perry <phil@elrepo.org> - 310.19-1.el5.elrepo
- Updated to version 310.19
- Drops support for older 6xxx and 7xxx series cards

* Sat Nov 10 2012 Philip J Perry <phil@elrepo.org> - 304.64-1.el5.elrepo
- Updated to version 304.64

* Fri Oct 19 2012 Philip J Perry <phil@elrepo.org> - 304.60-1.el5.elrepo
- Updated to version 304.60

* Fri Sep 28 2012 Philip J Perry <phil@elrepo.org> - 304.51-1.el5.elrepo
- Updated to version 304.51

* Tue Aug 28 2012 Philip J Perry <phil@elrepo.org> - 304.43-1.el5.elrepo
- Updated to version 304.43

* Tue Aug 14 2012 Philip J Perry <phil@elrepo.org> - 304.37-1.el5.elrepo
- Updated to version 304.37

* Wed Aug 08 2012 Philip J Perry <phil@elrepo.org> - 295.71-1.el5.elrepo
- Updated to version 295.71
- Fixes http://permalink.gmane.org/gmane.comp.security.full-disclosure/86747

* Tue Jun 19 2012 Philip J Perry <phil@elrepo.org> - 302.17-1.el5.elrepo
- Updated to version 302.17
- Built against kversion-2.6.18-308.el5

* Sat Jun 16 2012 Philip J Perry <phil@elrepo.org> - 295.59-1.el5.elrepo
- Updated to version 295.59

* Thu May 17 2012 Philip J Perry <phil@elrepo.org> - 295.53-1.el5.elrepo
- Updated to version 295.53

* Fri May 04 2012 Philip J Perry <phil@elrepo.org> - 295.49-1.el5.elrepo
- Updated to version 295.49

* Wed Apr 11 2012 Philip J Perry <phil@elrepo.org> - 295.40-1.el5.elrepo
- Updated to version 295.40
- Fixes CVE-2012-0946

* Fri Mar 23 2012 Philip J Perry <phil@elrepo.org> - 295.33-1.el5.elrepo
- Updated to version 295.33

* Mon Feb 13 2012 Philip J Perry <phil@elrepo.org> - 295.20-1.el5.elrepo
- Updated to version 295.20

* Wed Nov 23 2011 Philip J Perry <phil@elrepo.org> - 290.10-1.el5.elrepo
- Updated to version 290.10

* Fri Oct 07 2011 Philip J Perry <phil@elrepo.org> - 285.05.09-1.el5.elrepo
- Updated to version 285.05.09

* Tue Aug 02 2011 Philip J Perry <phil@elrepo.org> - 280.13-1.el5.elrepo
- Updated to version 280.13

* Fri Jul 22 2011 Philip J Perry <phil@elrepo.org> - 275.21-1.el5.elrepo
- Updated to version 275.21

* Fri Jul 15 2011 Philip J Perry <phil@elrepo.org> - 275.19-1.el5.elrepo
- Updated to version 275.19

* Fri Jun 17 2011 Philip J Perry <phil@elrepo.org> - 275.09.07-1.el5.elrepo
- Updated to version 275.09.07

* Sat Apr 16 2011 Philip J Perry <phil@elrepo.org> - 270.41.03-1.el5.elrepo
- Updated to version 270.41.03

* Fri Mar 25 2011 Philip J Perry <phil@elrepo.org>
- Updated to version 270.30 beta

* Fri Jan 21 2011 Philip J Perry <phil@elrepo.org> - 260.19.36-1.el5.elrepo
- Updated to version 260.19.36

* Thu Dec 16 2010 Philip J Perry <phil@elrepo.org> - 260.19.29-1.el5.elrepo
- Updated to version 260.19.29

* Wed Nov 10 2010 Philip J Perry <phil@elrepo.org> - 260.19.21-1.el5.elrepo
- Updated to version 260.19.21

* Fri Oct 15 2010 Philip J Perry <phil@elrepo.org> - 260.19.12-1.el5.elrepo
- Updated to version 260.19.12

* Fri Sep 10 2010 Philip J Perry <phil@elrepo.org> - 260.19.04-1.el5.elrepo
- Updated to version 260.19.04

* Tue Aug 31 2010 Philip J Perry <phil@elrepo.org> - 256.53-1.el5.elrepo
- Updated to version 256.53

* Sat Jul 30 2010 Philip J Perry <phil@elrepo.org> - 256.44-1.el5.elrepo
- Updated to version 256.44
- Added Conflicts for module-init-tools-3.3-0.pre3.1.60.el5 [BugID 0000064]
- Reverted earlier patch to kmodtool for update bug [BugID 0000064]

* Sat Jun 19 2010 Philip J Perry <phil@elrepo.org> - 256.35-1.el5.elrepo
- Updated to version 256.35

* Sat Jun 12 2010 Philip J Perry <phil@elrepo.org> - 195.36.31-1.el5.elrepo
- Updated to version 195.36.31.
- Patch kmodtool for update bug [BugID 0000064]
- Please see: http://elrepo.org/tiki/Update

* Fri Apr 23 2010 Philip J Perry <phil@elrepo.org> - 195.36.24-1.el5.elrepo
- Updated to version 195.36.24.
- Hard code kversion = 2.6.18-164.el5.
- Update kmodtool to latest version.

* Sat Mar 20 2010 Philip J Perry <phil@elrepo.org> - 195.36.15-1.el5.elrepo
- Updated to version 195.36.15.

* Sun Feb 21 2010 Philip J Perry <phil@elrepo.org> - 190.53-1.el5.elrepo
- Updated to version 190.53.
- Updated kmodtool to latest version.

* Sun Nov 01 2009 Philip J Perry <phil@elrepo.org> - 190.42-1.el5.elrepo
- Updated to version 190.42.

* Sat Sep 12 2009 Akemi Yagi <toracat@elrepo.org> - 185.18.36-2.el5.elrepo
- kernel-modules in kmodtool changed to kabi-modules

* Sat Sep 12 2009 Akemi Yagi <toracat@elrepo.org> - 185.18.36-1.el5.elrepo
- Update to version 185.18.36.

* Mon Aug 17 2009 Philip J Perry <phil@elrepo.org> - 185.18.31-2.el5.elrepo
- Added Provides: kabi-modules as kABI looks stable for RHEL5.4.

* Mon Aug 10 2009 Philip J Perry <phil@elrepo.org> - 185.18.31-1.el5.elrepo
- Update to version 185.18.31.
- Drop bundling the source here too, it's included in nvidia-x11-drv.

* Fri Jul 10 2009 Philip J Perry <phil@elrepo.org> - 185.18.14-1.el5.elrepo
- Rebuilt against kernel-2.6.18-128.el5 for release.
- Updated kmodtool to latest specification.
- Create nvidia.conf in spec file.

* Tue Jul 07 2009 Philip J Perry <phil@elrepo.org>
- Updated sources to match nvidia-x11-drv.
- Fixed paths to extract sources.
- Don't strip the module (NVIDIA doesn't).

* Wed Jun 10 2009 Alan Bartlett <ajb@elrepo.org>
- Updated the package to 185.18.14 version.

* Thu May 21 2009 Dag Wieers <dag@wieers.com>
- Adjusted the package name.

* Tue May 19 2009 Alan Bartlett <ajb@elrepo.org>
- Total revision and re-build of the kmod packages.

* Thu May 14 2009 Alan Bartlett <ajb@elrepo.org>
- Initial build of the kmod packages.
