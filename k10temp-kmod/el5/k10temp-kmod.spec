# Define the kmod package name here.
%define	 kmod_name k10temp

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.18-238.el5}

Name:	 %{kmod_name}-kmod
Version: 0.0
Release: 4%{?dist}
Group:	 System Environment/Kernel
License: GPLv2
Summary: AMD K10 core temperature monitor driver module
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
This package provides the AMD K10 core temperature monitor driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the CentOS kernel and not on any one specific build.

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
%{__install} %{kmod_name}.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Sun Aug 08 2011 Philip J Perry <phil@elrepo.org> - 0.0-4.el5.elrepo
- Add the docs.
- Rebase to kernel-3.0.1
- Add support for AMD Family 15h (Bulldozer) CPUs [2011-05-25]
- Use helper functions to set and get driver data [2011-05-25]

* Sun Apr 24 2011 Philip J Perry <phil@elrepo.org> - 0.0-3.el5.elrepo
- Rebased to kernel-2.6.38.4
- Add support for AMD Family 12h/14h CPUs [2011-02-18]

* Thu Jul 16 2009 Philip J Perry <phil@elrepo.org> - 0.0-2.el5.elrepo
- Rebuilt for ELRepo release.

* Thu Jul 16 2009 Akemi Yagi <toracat@elrepo.org>
- Fixed K8 > K10 typos in package description.

* Sat Jul 11 2009 Wes Wright <info@riversedgesoftware.com>
- Wrap the public k10temp into kmod
