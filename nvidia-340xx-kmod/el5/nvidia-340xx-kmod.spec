# Define the kmod package name here.
%define	 kmod_name nvidia-340xx

# If kversion isn't defined on the rpmbuild line, define it here.
# kABI compatible with kernel 2.6.18-194.el5 upwards
%{!?kversion: %define kversion 2.6.18-398.el5}

Name:    %{kmod_name}-kmod
Version: 340.96
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
echo "override nvidia * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override nvidia-uvm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf

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
    pushd _kmod_build_$kvariant/kernel/uvm
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
    pushd _kmod_build_$kvariant/kernel/uvm
    %{__make} -C "${ksrc}" modules_install M=$PWD
    popd
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -p -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Nov 20 2015 Philip J Perry <phil@elrepo.org> - 340.96-1.el5.elrepo
- Updated to version 340.96

* Sat Sep 12 2015 Philip J Perry <phil@elrepo.org> - 340.93-1.el5.elrepo
- Updated to version 340.93

* Thu Feb 05 2015 Philip J Perry <phil@elrepo.org> - 340.76-1.el5.elrepo
- Updated to version 340.76

* Tue Dec 16 2014 Philip J Perry <phil@elrepo.org> - 340.65-1.el5.elrepo
- Updated to version 340.65

* Fri Sep 26 2014 Philip J Perry <phil@elrepo.org> - 340.32-1.el5.elrepo
- Fork to legacy release nvidia-340xx
- Trimmed changelog

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

