# $Id$
# Authority: dag

### Version 1.4.7 needs at least kernel 2.6.18-92.el5
%define kversion 2.6.18-92.el5

# Define the kmod package name here.
%define kmod_name ocfs2

Summary: OCFS2 driver module
Name: %{kmod_name}-kmod
Version: 1.4.7
Release: 1%{?dist}
License: GPL v2
Group: System Environment/Kernel
URL: http://oss.oracle.com/projects/ocfs2/

# Sources.
Source0: http://oss.oracle.com/projects/ocfs2/dist/files/source/v1.4/%{kmod_name}-%{version}.tar.gz
Source10: kmodtool-%{kmod_name}
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

ExclusiveArch: i686 x86_64

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
This package provides the kernel OCFS2 driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the CentOS kernel and not on any one specific build.

OCFS2 is a POSIX-compliant shared-disk cluster file system for Linux
capable of providing both high performance and high availability.

%prep
%setup -c -T -a 0
for kvariant in %{kvariants} ; do
    %{__cp} -a %{kmod_name}-%{version} _kmod_build_$kvariant
    %{__cat} <<-EOF >_kmod_build_$kvariant/fs/%{kmod_name}.conf
	override %{kmod_name} * weak-updates/%{kmod_name}
	override %{kmod_name}_nodemanager * weak-updates/%{kmod_name}/cluster
	override %{kmod_name}_dlm * weak-updates/%{kmod_name}/dlm
	override %{kmod_name}_dlmfs * weak-updates/%{kmod_name}/dlm
	EOF
    pushd _kmod_build_$kvariant
    %configure --with-vendor="rhel5" --with-vendorkernel="%{kversion}"
    popd
done

%build
for kvariant in %{kvariants} ; do
    ksrc="%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}"
    pushd _kmod_build_$kvariant/fs
    %{__make} -C "${ksrc}" modules M="$PWD"
    popd
done

%install
%{__rm} -rf %{buildroot}
export INSTALL_MOD_PATH="%{buildroot}"
export INSTALL_MOD_DIR="extra"
for kvariant in %{kvariants} ; do
    ksrc="%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}"
    pushd _kmod_build_$kvariant/fs
    %{__make} -C "${ksrc}" modules_install M="$PWD"
    %{__install} -Dp -m0644 %{kmod_name}.conf %{buildroot}/etc/depmod.d/%{kmod_name}.conf
    popd
done
# Strip the module(s).
find %{buildroot} -type f -name \*.ko -exec strip --strip-debug \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Wed Jun 16 2010 Dag Wieers <dag@elrepo.org> - 1.4.7-1
- Updated to release 1.4.7.

* Wed Aug 12 2009 Philip J Perry <phil@elrepo.org> - 1.4.2-2
- Fixed dependencies.
- Fixed install path.
- Fixed configure options.

* Tue Aug 04 2009 Philip J Perry <phil@elrepo.org> - 1.4.2-1
- Initial build of the kmod package.
