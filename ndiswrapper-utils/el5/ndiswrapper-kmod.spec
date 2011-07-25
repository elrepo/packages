# $Id$
# Authority: dag

%define kversion 2.6.18-8.el5

# Define the kmod package name here.
%define kmod_name ndiswrapper

Summary: %{kmod_name} kernel modules
Name: %{kmod_name}-kmod
Version: 1.56
Release: 1%{?dist}
License: GPL v2
Group: System Environment/Kernel
URL: http://ndiswrapper.sourceforge.net/

# Sources.
Source0: http://heanet.dl.sourceforge.net/project/ndiswrapper/stable/%{version}/ndiswrapper-%{version}.tar.gz
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
This package provides the %{kmod_name} kernel modules.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -c -T -a 0
for kvariant in %{kvariants} ; do
    %{__cp} -a %{kmod_name}-%{version} _kmod_build_$kvariant
    %{__cat} <<-EOF >_kmod_build_$kvariant/driver/%{kmod_name}.conf
	override %{kmod_name} * weak-updates/%{kmod_name}
	EOF
done

%build
for kvariant in %{kvariants} ; do
    ksrc="%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}"
    %{__make} -C _kmod_build_$kvariant KVERS="%{kversion}" KBUILD="${ksrc}"
done

%install
%{__rm} -rf %{buildroot}

export INSTALL_MOD_PATH="%{buildroot}"
export INSTALL_MOD_DIR="extra/%{kmod_name}"
for kvariant in %{kvariants} ; do
    ksrc="%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}"
    pushd _kmod_build_$kvariant/driver
    %{__make} -C "${ksrc}" modules_install M="$PWD"
    %{__install} -Dp -m0644 %{kmod_name}.conf %{buildroot}/etc/depmod.d/%{kmod_name}.conf
    popd
done
# Strip the module(s).
find %{buildroot} -type f -name \*.ko -exec strip --strip-debug \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Wed Jun 16 2010 Dag Wieers <dag@elrepo.org> - 1.56-1
- Updated to release 1.56.

* Fri Oct 23 2009 Alan Bartlett <ajb@elrepo.org> - 1.54-4
- Revised the kmodtool file and this spec file.

* Fri Oct 09 2009 Alan Bartlett <ajb@elrepo.org> - 1.54-3
- Revised the kmodtool file and this spec file.

* Mon Aug 17 2009 Alan Bartlett <ajb@elrepo.org> - 1.54-2
- Revised the kmodtool file and this spec file.

* Thu May 07 2009 Alan Bartlett <ajb@elrepo.org> - 1.54-1
- Initial build of the kmod packages.

