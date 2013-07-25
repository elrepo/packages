# Define the kmod package name here.
%define	 kmod_name w83627ehf

# If kversion isn't defined on the rpmbuild line, define it here.
# Only compatible with kernels >= 2.6.18-194.el5
%{!?kversion: %define kversion 2.6.18-348.el5}

Name:	 %{kmod_name}-kmod
Version: 0.0
Release: 9%{?dist}
Group:	 System Environment/Kernel
License: GPLv2
Summary: w83627ehf kernel module
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
This package provides the kernel driver module for the w83627ehf sensor.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
for kvariant in %{kvariants} ; do
    %{__cp} -a %{kmod_name}-%{version} _kmod_build_$kvariant
done
%{__cp} -a %{SOURCE5} .
%{__cp} -a %{SOURCE6} .
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
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{kmod_name}.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Jul 25 2013 Philip J Perry <phil@elrepo.org> - 0.0-9.el5.elrepo
- Rebase to LTS kernel-3.2.46
- Adds support for W83627UHG
  [http://elrepo.org/bugs/view.php?id=386]

* Thu Apr 19 2012 Philip J Perry <phil@elrepo.org> - 0.0-8.el5.elrepo
- Rebase to LTS kernel-3.0.28
- Fix memory leak in probe function [2012-03-19]
- Fix writing into fan_stop_time for... [2012-03-19]
- Fix number of fans for NCT6776F [2012-02-13]
- Disable setting DC mode for pwm2... [2012-02-03]
- Fix broken driver init [2011-11-11]
- Properly report PECI and AMD-SI... [2011-11-11]
- Fix negative 8-bit temperature values [2011-10-25]
- Properly report thermal diode sensors [2011-10-25]

* Sun Jun 05 2011 Philip J Perry <phil@elrepo.org> - 0.0-7.el5.elrepo
- Rebase to kernel-2.6.39.1
- Install the docs.
- Fix implicit declaration of function 'DIV_ROUND_CLOSEST' on RHEL5
- Display correct temperature sensor... [2011-03-15]
- Add fan debounce support for NCT6775... [2011-03-15]
- Store rpm instead of raw fan speed... [2011-03-15]
- Use 16 bit fan count registers if... [2011-03-15]
- Add support for Nuvoton NCT6775F... [2011-03-15]
- Permit enabling SmartFan IV mode... [2011-03-15]
- Convert register arrays to 16 bit... [2011-03-15]
- Improve support for W83667HG-B [2011-03-15]
- Optimize multi-bank register access [2011-03-15]
- Fixed most checkpatch warnings and... [2011-03-15]
- Unify temperature register access... [2011-03-15]
- Use pr_fmt and pr_<level> [2011-01-08]

* Sun Feb 06 2011 Philip J Perry <phil@elrepo.org> - 0.0-6.el5.elrepo
- Rebase to kernel-2.6.37
- Fix implicit declaration of function 'acpi_check_resource_conflict' on RHEL5
- Use proper exit sequence [2010-09-17]
- Add support for W83667HG-B [2010-08-14]
- Driver cleanup [2010-08-14]

* Wed Sep 22 2010 Philip J Perry <phil@elrepo.org> - 0.0-5.el5.elrepo
- Update to latest elrepo specifications.
- Fixed update bug [BugID 0000064]

* Mon Jun 14 2010 Philip J Perry <phil@elrepo.org> - 0.0-4.el5.elrepo
- Update to 2.6.34
- w83627ehf updates [2009-12-15]
- kABI compatible with el5.5 upwards, for earlier kernels use an older release
- Patch kmodtool for update bug [BugID 0000064]
- Please see: http://elrepo.org/tiki/Update

* Fri Feb 05 2010 Philip J Perry <phil@elrepo.org> - 0.0-3.el5.elrepo
- Fix Include <linux/io.h> [2009-09-15]

* Mon Aug 24 2009 Philip J Perry <phil@elrepo.org> - 0.0-2.el5.elrepo
- Update to kernel-2.6.31-rc7
- Add W83627DHG-P support [2009-06-15]

* Mon Aug 10 2009 Philip J Perry <phil@elrepo.org> - 0.0-1.el5.elrepo
- Initial backport of driver from kernel-2.6.30.4
- Revert check for ACPI resource conflicts patch [2009-01-07]
- Convert hwmon_device_register/unregister to class_device [2007-10-10]
