# Define the kmod package name here.
%define	 kmod_name fuse

Name:	 %{kmod_name}-kmod
Version: 2.7.4
Release: 3.el5.elrepo
Group:	 System Environment/Kernel
License: GPL v2
Summary: %{kmod_name} kernel modules
URL:	 http://fuse.sourceforge.net/

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
This package provides the kernel modules for FUSE support, with which it is
possible to implement a fully functional filesystem in a userspace program.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    %{__cp} -a %{kmod_name}-%{version} _kmod_build_$kvariant
    pushd _kmod_build_$kvariant/kernel
    ./configure -q --enable-kernel-module --with-kernel="${ksrc}"
    %{__cat} <<-EOF > %{kmod_name}.conf
	override %{kmod_name} * weak-updates/%{kmod_name}
	EOF
    popd
done

%build
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant/kernel
    %{__make} -C "${ksrc}" modules M=$PWD
    popd
done

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=extra/%{kmod_name}
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant/kernel
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

* Sun Aug 09 2009 Alan Bartlett <ajb@elrepo.org>
- Revised the kmodtool file and this spec file.

* Wed May 06 2009 Alan Bartlett <ajb@elrepo.org>
- Initial build of the kmod packages.

