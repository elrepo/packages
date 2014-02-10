# Define the kmod package name here.
%define	 kmod_name nvidia-173xx

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.18-371.el5}

Name:	 %{kmod_name}-kmod
Version: 173.14.39
Release: 1%{?dist}
Group:	 System Environment/Kernel
License: Proprietary
Summary: NVIDIA 173.14.xx OpenGL kernel driver module
URL:	 http://www.nvidia.com/

BuildRequires:	rpm-build, redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0: ftp://download.nvidia.com/XFree86/Linux-x86/%{version}/NVIDIA-Linux-x86-%{version}-pkg0.run
Source1: ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}-pkg2.run
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
%define kmodtool sh %{SOURCE10}
%{expand:%(%{kmodtool} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants} 2>/dev/null)}

# Disable the building of the debug package(s).
%define	debug_package %{nil}

# Define the filter.
%define __find_requires sh %{_builddir}/%{buildsubdir}/filter-requires.sh

%description
This package provides the proprietary NVIDIA 173.14.xx OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh

%ifarch i686
sh %{SOURCE0} --extract-only --target nvidiapkg
%endif

%ifarch x86_64
sh %{SOURCE1} --extract-only --target nvidiapkg
%endif

for kvariant in %{kvariants} ; do
    %{__cp} -a nvidiapkg _kmod_build_$kvariant
    %{__cat} <<-EOF > _kmod_build_$kvariant/usr/src/nv/%{kmod_name}.conf
	override nvidia * weak-updates/%{kmod_name}
	EOF
done

%build
for kvariant in %{kvariants} ; do
    export SYSSRC=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant/usr/src/nv
    %{__make} module
    popd
done

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=extra/%{kmod_name}
for kvariant in %{kvariants} ; do
    pushd _kmod_build_$kvariant/usr/src/nv
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    %{__make} -C "${ksrc}" modules_install M=$PWD
    %{__mkdir_p} ${INSTALL_MOD_PATH}/etc/depmod.d
    %{__install} -p -m 0644 %{kmod_name}.conf ${INSTALL_MOD_PATH}/etc/depmod.d/
    popd
done

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%changelog
* Mon Feb 10 2014 Philip J Perry <phil@elrepo.org> - 173.14.39-1.el5.elrepo
- Update to version 173.14.39.

* Sat Mar 02 2013 Philip J Perry <phil@elrepo.org> - 173.14.36-1.el5.elrepo
- Update to version 173.14.36.

* Sat Aug 21 2010 Philip J Perry <phil@elrepo.org> - 173.14.27-1.el5.elrepo
- Update to version 173.14.27.

* Thu Feb 04 2010 Philip J Perry <phil@elrepo.org> - 173.14.25-2.el5.elrepo
- Rebuilt to match nvidia-x11-drv-173xx.
- Requires kernel >= 2.6.18-128.el5

* Wed Feb 03 2010 Philip J Perry <phil@elrepo.org> - 173.14.25-1.el5.elrepo
- Forked to create kmod-nvidia-173xx legacy release.

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
