# Define the kmod package name here.
%define	 kmod_name coretemp

# If kversion isn't defined on the rpmbuild line, define it here.
# kmod-coretemp is kABI compatible with kernel >= 2.6.18-194.el5
# when built against kernel 2.6.18-238.el5
%{!?kversion: %define kversion 2.6.18-238.el5}

Name:	 %{kmod_name}-kmod
Version: 1.1
Release: 10%{?dist}
Group:	 System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module
URL:	 http://www.kernel.org/

BuildRequires:	redhat-rpm-config
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
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

# If kvariants isn't defined on the rpmbuild line, build all variants for this architecture.
%{!?kvariants: %define kvariants %{?basevar} %{?paevar}}

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate_kmp %{kmod_name} %{kversion} %{kvariants})}

# Disable the building of the debug package(s).
%define	debug_package %{nil}

# Define the filter.
%define __find_requires sh %{_builddir}/%{buildsubdir}/filter-requires.sh

%description
This package provides the kernel module for monitoring the temperature of Intel Core series CPUs.
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
    KSRC=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    pushd _kmod_build_$kvariant
    %{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD
    popd
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
%{__install} %{kmod_name}.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Nov 18 2011 Philip J Perry <phil@elrepo.org> - 1.1-10.el5.elrepo
- Backported from kernel-2.6.39.4
- Include patches through [2011-06-23]
- Install the docs

* Sat Feb 19 2011 Philip J Perry <phil@elrepo.org> - 1.1-9.el5.elrepo
- Backported from kernel-2.6.37.1
- kABI compatible with kernel >= 2.6.18-238.el5
- No longer compatible with xen kernels
- Include patches through [2010-10-25]
- Revert "fix initialization of coretemp" [2010-09-24]
- Revert "register alternate sibling upon CPU removal" [2010-09-24]

* Sat Aug 14 2010 Philip J Perry <phil@elrepo.org> - 1.1-8.el5.elrepo
- Rebase to kernel-2.6.35.2
- Add Conflicts for module-init-tools-3.3-0.pre3.1.60.el5 [BugID 0000064]
- Properly label the sensors [2010-07-09]
- Skip duplicate CPU entries, fixed to build on el5 [2010-07-09]
- Get TjMax value from MSR [2010-05-25]
- Detect the thermal sensors by CPUID [2010-05-25]

* Mon Jun 14 2010 Philip J Perry <phil@elrepo.org> - 1.1-7.el5.elrepo
- Rebase to kernel-2.6.34
- Add missing newline to dev_warn() message [2010-03-29]
- Fix cpu model output [2010-03-29]
- Fix TjMax for Atom N450/D410/D510 CPUs [2010-01-10]
- Patch kmodtool for update bug [BugID 0000064]
- Please see: http://elrepo.org/tiki/Update

* Mon Oct 19 2009 Philip J Perry <phil@elrepo.org> - 1.1-6.el5.elrepo
- Add support for Intel Atom CPUs [coretemp-intel-atom.patch]
- Add support for mobile Penryn CPU [coretemp-penryn-mobile.patch]
- Add support for Lynnfield CPU [coretemp-lynnfield.patch]

* Wed Aug 19 2009 Philip J Perry <phil@elrepo.org> - 1.1-5.el5.elrepo
- Update handling of coretemp.conf in SPEC file.
- Add Provides: kabi-modules.

* Thu May 21 2009 Philip J Perry <phil@elrepo.org> - 1.1-4.el5.elrepo
- Rebuilt for lm_sensors dependency.
- Fixed spec file to strip module.

* Sat Mar 28 2009 Philip J Perry <phil@elrepo.org> - 1.1-3.el5.elrepo
- Rebuilt for elrepo release.

* Fri Dec 19 2008 Philip J Perry <ned@unixmail.co.uk>
- Rebased to kernel-2.6.27.10 driver. 1.1-2.el5
- Added MSR patch <coretemp-msr.patch>
- Added 2.6.18 build compatibility patch <coretemp-compat-2.6.18.patch>

* Sat Dec 06 2008 Philip J Perry <ned@unixmail.co.uk>
- Rebased to kernel-2.6.27.8 driver. 1.1-1.el5
- Adds support for Penryn and Nehalem (i7) CPUs

* Mon Oct 27 2008 Alan J Bartlett <ajb.stxsl@gmail.com>
- Total revision of the spec file. 1.1-1

* Tue Jul 15 2008 Alan J Bartlett <ajb.stxsl@gmail.com>
- Fixed bugs in spec file. 1.0-4.92.1.6.el5

* Sat Jul 12 2008 Philip J Perry <ned@unixmail.co.uk>
- Fixed dependencies. 1.0-3.92.1.6.el5

* Tue Jul 08 2008 Philip J Perry <ned@unixmail.co.uk>
- Fixed bug in spec file. 1.0-2.92.1.6.el5

* Tue Jul 08 2008 Philip J Perry <ned@unixmail.co.uk>
- Initial RPM build. 1.0-1.92.1.6.el5
