# Define the kmod package name here.
%define	 kmod_name ntfs

Name:	 %{kmod_name}-kmod
Version: 2.1.27
Release: 3.el5.elrepo
Group:	 System Environment/Kernel
License: GPL v2
Summary: kernel modules for %{kmod_name} support
URL:	 http://www.kernel.org/

BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.bz2
Source10: kmodtool-%{kmod_name}

# If kversion isn't defined on the rpmbuild line, build for the current kernel.
%{!?kversion: %define kversion %(uname -r)}

# Define the variants for each architecture.
%define basevar ""
%ifarch i686
%define paevar PAE
%endif
%ifarch i686 x86_64
%define xenvar xen
%endif

# If kvariants isn't defined on the rpmbuild line, build all variants for this architecture.
%{!?kvariants: %define kvariants %{?basevar} %{?xenvar} %{?paevar}}

# Magic hidden here.
%define kmodtool sh %{SOURCE10}
%{expand:%(%{kmodtool} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants} 2>/dev/null)}

%description
This package provides the kernel modules for %{kmod_name} support.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
for kvariant in %{kvariants} ; do
    %{__cp} -a %{kmod_name}-%{version} _kmod_build_$kvariant
    %{__cat} <<-EOF >_kmod_build_$kvariant/%{kmod_name}.conf
	override %{kmod_name} * weak-updates/%{kmod_name}
	EOF
done

%build
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant
    %{__make} -C "${ksrc}" modules M=$PWD
    popd
done

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=extra/%{kmod_name}
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant
    %{__make} -C "${ksrc}" modules_install M=$PWD
    %{__install} -d ${INSTALL_MOD_PATH}/etc/depmod.d/
    %{__install} %{kmod_name}.conf ${INSTALL_MOD_PATH}/etc/depmod.d/
    popd
done
# Strip the module(s).
find ${INSTALL_MOD_PATH} -type f -name \*.ko -exec strip --strip-debug \{\} \;

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%changelog
* Fri Oct 09 2009 Alan Bartlett <ajb@elrepo.org>
- Revised the kmodtool file and this spec file.

* Wed Aug 12 2009 Alan Bartlett <ajb@elrepo.org>
- Revised the kmodtool file and this spec file.

* Mon Apr 20 2009 Alan Bartlett <ajb@elrepo.org>
- Version for public release.

* Sat Mar 28 2009 Alan Bartlett <ajb@elrepo.org>
- Add code to strip the module(s).
- Add the .elrepo tag to the release.
- Tidy up this spec file.

* Sun Mar 08 2009 Akemi Yagi <toracat@elrepo.org>
- Initial build of the kmod packages.
- Original ntfs code in CentOS-5.3 is broken.
- Suggested patch provided by Eric Sandeen [bugzilla 481495] [centos bug 3363].
- Patched code and Makefile provided by Alan Bartlett.

