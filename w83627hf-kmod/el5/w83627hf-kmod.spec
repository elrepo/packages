# Define the kmod package name here.
%define	 kmod_name w83627hf

# If kversion isn't defined on the rpmbuild line, define it here.
# Only compatible with kernels >= 2.6.18-194.el5
%{!?kversion: %define kversion 2.6.18-308.el5}

Name:	 %{kmod_name}-kmod
Version: 0.0
Release: 5%{?dist}
Group:	 System Environment/Kernel
License: GPLv2
Summary: w83627hf driver module
URL:	 http://www.kernel.org/

BuildRequires:	redhat-rpm-config
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:	i686 x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.bz2
Source5:  GPL-v2.0.txt
Source6:  %{kmod_name}.txt
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
%define	debug_package %{nil}

# Define the filter.
%define __find_requires sh %{_builddir}/%{buildsubdir}/filter-requires.sh

%description
This package provides the kernel driver module for the w83627hf sensor.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
for kvariant in %{kvariants} ; do
    %{__cp} -a %{kmod_name}-%{version} _kmod_build_$kvariant
done
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant
    %{__make} -C "${ksrc}" %{?_smp_mflags} modules M=$PWD
    popd
done

%install
%{__rm} -rf %{buildroot}
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
for kvariant in %{kvariants} ; do
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant
    %{__make} -C "${ksrc}" modules_install M=$PWD
    popd
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE6} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Apr 19 2012 Philip J Perry <phil@elrepo.org> - 0.0-5.el5.elrepo
- Rebase to LTS kernel-3.0.28
- Use pr_fmt and pr_<level> [2011-01-08]
- Install the docs

* Sun Feb 06 2011 Philip J Perry <phil@elrepo.org> - 0.0-4.el5.elrepo
- Rebase to kernel-2.6.37
- Fix implicit declaration of function 'acpi_check_resource_conflict' on RHEL5
- Fixed update bug [BugID 0000064]

* Mon Jun 14 2010 Philip J Perry <phil@elrepo.org> - 0.0-3.el5.elrepo
- Update to 2.6.34
- Fix for "No such device" [2009-12-16]
- Stop using globals for I/O port numbers [2009-12-09]
- Drop the force_addr module parameter [2009-12-09]
- kABI compatible with el5.5 upwards, for earlier kernels use an older release
- Patch kmodtool for update bug [BugID 0000064]
- Please see: http://elrepo.org/tiki/Update

* Fri Feb 05 2010 Philip J Perry <phil@elrepo.org> - 0.0-2.el5.elrepo
- Fix Include <linux/io.h> [2009-09-15]

* Wed Aug 19 2009 Philip J Perry <phil@elrepo.org> - 0.0-1.el5.elrepo
- Initial backport of driver from kernel-2.6.30.5
- Revert check for ACPI resource conflicts patch [2009-01-07]
- Convert hwmon_device_register/unregister to class_device [2007-10-10]
