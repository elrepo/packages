# Define the kmod package name here.
%define	 kmod_name it87

# If kversion isn't defined on the rpmbuild line, define it here.
# Only compatible with kernels >= 2.6.18-194.el5
%{!?kversion: %define kversion 2.6.18-194.el5}

Name:	 %{kmod_name}-kmod
Version: 1.1
Release: 12%{?dist}
Group:	 System Environment/Kernel
License: GPLv2
Summary: IT87 Super I/O Sensor module
URL:	 http://www.kernel.org/

BuildRequires: redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

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
%define debug_package %{nil}

# Define the filter.
%define __find_requires sh %{_builddir}/%{buildsubdir}/filter-requires.sh

%description
This package provides the %{kmod_name} kernel module(s).
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
    %{__make} -C "${ksrc}" modules M=$PWD
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
* Wed Aug 28 2013 Philip J Perry <phil@elrepo.org> - 1.1-12.el5.elrepo
- Rebase to kernel-3.10.9.
- Adds support for IT8782F, IT8783E/F, IT8771E and IT8772E.
- Backport functions to build on RHEL5_9

* Thu Jul 25 2013 Philip J Perry <phil@elrepo.org> - 1.1-11.el5.elrepo
- Rebase to kernel-3.0.87.
- Preserve configuration register bits on init [2012-07-19]

* Thu Mar 15 2012 Philip J Perry <phil@elrepo.org> - 1.1-10.el5.elrepo
- Rebase to kernel-3.0.24.
- Fix label group removal [2011-07-17]
- Backport support for IT8728F from kernel-3.3-rc7
  http://elrepo.org/bugs/view.php?id=246

* Sun Aug 07 2011 Philip J Perry <phil@elrepo.org> - 1.1-9.el5.elrepo
- add the docs.
- Rebase to kernel-2.6.39.4
- Fix label group removal [2011-08-03]
- Use pr_fmt and pr_<level> [2011-01-12]

* Sun Feb 06 2011 Philip J Perry <phil@elrepo.org> - 1.1-8.el5.elrepo
- Rebase to kernel-2.6.37
- Fix implicit declaration of function 'acpi_check_resource_conflict' on RHEL5
- Fix implicit declaration of function 'DIV_ROUND_CLOSEST' on RHEL5
- Export labels for internal sensors [2010-08-14]
- Move conversion functions [2010-10-28]
- Add support for the IT8721F/IT8758E [2010-10-28]
- Fix manual fan speed control on IT8721F [2010-12-08]

* Sat Aug 14 2010 Philip J Perry <phil@elrepo.org> - 1.1-7.el5.elrepo
- Add Conflicts for module-init-tools-3.3-0.pre3.1.60.el5 [BugID 0000064]
- Rebase to kernel-2.6.35.2
- Fix implicit declaration of functions 'pr_notice' and 'pr_fmt' on RHEL5
- Fix in7 on IT8720F [2010-07-09]

* Tue May 18 2010 Philip J Perry <phil@elrepo.org> - 1.1-6.el5.elrepo
- Rebase to kernel-2.6.34.
- Rebuild against 2.6.18-194.el5
- Now only kABI-compatible with RHEL-5.5 series kernels.

* Tue Mar 09 2010 Philip J Perry <phil@elrepo.org> - 1.1-5.el5.elrepo
- Rebase module to kernel-2.6.33.
- Add 'Check for fan2 and fan3 availability' patch [2009-12-09]
- Add 'Verify the VID pin usage' patch [2009-12-09]
- Revert upstream 'Check for ACPI resource conflicts' patch [2009-01-07]

* Wed Feb 03 2010 Philip J Perry <phil@elrepo.org> - 1.1-4.el5.elrepo
- Backport fix VID reading on IT8718F/IT8720F [2009-10-24]
- Update kmodtool to latest spec.

* Sun Sep 06 2009 Philip J Perry <phil@elrepo.org> - 1.1-3.el5.elrepo
- Fix Include <linux/io.h> [2009-09-03]
- Update kmodtool to latest spec.

* Wed Aug 19 2009 Philip J Perry <phil@elrepo.org> - 1.1-2.el5.elrepo
- Update handling of it87.conf in SPEC file.
- Add Provides: kabi-modules.

* Mon May 04 2009 Philip J Perry <phil@elrepo.org> - 1.1-1.el5.elrepo
- Rebase module to kernel-2.6.29.2.
- Revert upstream 'Check for ACPI resource conflicts' patch [2009-01-07]
- Adds support for IT8720F and IT8726F

* Mon May 04 2009 Alan Bartlett <ajb@elrepo.org>
- Added code to strip the module.
- Aligned this spec file to the ElRepo standards.

* Sat Mar 28 2009 Philip J Perry <phil@elrepo.org> - 1.0-4.el5.elrepo
- Rebuilt for elrepo release.

* Fri Dec 26 2008 Philip J Perry <ned@unixmail.co.uk> - 1.0-3.el5
- Rebuilt against base CentOS-5 kernel for release.

* Sun Jul 27 2008 Philip J Perry <ned@unixmail.co.uk> - 1.0-2.92.1.6.el5
- Fixed weak-updates priority override in kmodtool-it87.

* Tue Jul 22 2008 Philip J Perry <ned@unixmail.co.uk> - 1.0-1.92.1.6.el5
- Initial RPM build.
- Backport from kernel-2.6.22.19 adding support for IT8716F and IT8718F.
