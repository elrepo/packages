# Define the kmod package name here.
%define	 kmod_name nvidia

# If kversion isn't defined on the rpmbuild line, define it here.
# kABI compatible with kernel 2.6.18-128.el5 upwards
%{!?kversion: %define kversion 2.6.18-164.el5}

Name:    %{kmod_name}-kmod
Version: 290.10
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

%description
This package provides the proprietary NVIDIA OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

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
    pushd _kmod_build_$kvariant/kernel
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    %{__make} -C "${ksrc}" modules_install M=$PWD
    popd
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -p -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/

%clean
%{__rm} -rf %{buildroot}

%changelog
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
