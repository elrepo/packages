# Define the kmod package name here.
%define kmod_name mhvtl

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.18-308.el5}

Summary: Virtual Tape Library device driver
Name: %{kmod_name}-kmod
%define real_version 2012-09-13
%define tar_version 1.4
Version: 1.4.4
Release: 1%{?dist}
Group: System Environment/Kernel
License: GPLv2
URL: http://sites.google.com/site/linuxvtl2/

BuildRequires: redhat-rpm-config
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0: mhvtl-%{real_version}.tgz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el5.sh

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
%{expand:%(sh %{SOURCE10} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants})}

# Disable the building of the debug package(s).
%define debug_package %{nil}

# Define the filter.
%define __find_requires sh %{_builddir}/%{buildsubdir}/filter-requires.sh

%description
This package provides the Virtual Tape Library device driver module for
Linux.  It is built to depend upon the specific ABI provided by a range
of releases of the same variant of the Linux kernel and not on any one
specific build.

%prep
%setup -c -T -a 0
for kvariant in %{kvariants} ; do
    %{__cp} -a %{kmod_name}-%{tar_version}/kernel/ _kmod_build_$kvariant
done
%{__cp} -a %{SOURCE5} .
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
for kvariant in %{kvariants} ; do
    KSRC=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    %{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD/_kmod_build_$kvariant
done

%install
%{__rm} -rf %{buildroot}
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
for kvariant in %{kvariants} ; do
    KSRC=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    %{__make} -C "${KSRC}" modules_install M=$PWD/_kmod_build_$kvariant
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Sep 14 2012 Dag Wieers <dag@wieers.com> - 1.4.4-1
- Updated to release 1.4-4 (2012-09-13).

* Thu Jun 21 2012 Dag Wieers <dag@wieers.com> - 1.3-1
- Updated to release 1.3 (2012-06-15).

* Thu Aug 05 2010 Dag Wieers <dag@wieers.com> - 0.18-11
- Initial el5 build of the kmod package.
