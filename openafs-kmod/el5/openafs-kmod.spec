# Define the kmod package name here.
%define  kmod_name openafs

# Define the OpenAFS sysname
%ifarch %{ix86} 
%define sysname i386_linux26
%endif
%ifarch ppc
%define sysname ppc_linux26
%endif
%ifarch ppc64
%define sysname ppc64_linux26
%endif
%ifarch x86_64
%define sysname amd64_linux26
%endif

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.18-8.el5}

Name:    %{kmod_name}-kmod
Version: 1.6.5
Release: 1%{?dist}
Group:   System Environment/Kernel
License: IBM
Summary: %{kmod_name} kernel module(s)
URL:     http://www.openafs.org

BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i586 i686 x86_64 ppc ppc64
BuildRequires:  pam-devel, ncurses-devel, flex, byacc, bison, automake
BuildRequires:  redhat-rpm-config

# Sources.
Source0:  	http://www.openafs.org/dl/openafs/1.4.12/%{kmod_name}-%{version}-src.tar.bz2
Source5:	LICENSE
Source10: 	kmodtool-%{kmod_name}-el5.sh

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
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
for kvariant in %{kvariants} ; do
    pushd _kmod_build_$kvariant
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}

    %{configure} --with-afs-sysname=%{sysname} --enable-kernel-module \
        --disable-linux-syscall-probing  \
        --with-linux-kernel-headers="${ksrc}"
    %{__make} MPS=MP only_libafs
    popd
done


%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT

for kvariant in %{kvariants} ; do
    INSTALL_MOD_DIR=/lib/modules/%{kversion}${kvariant}/extra/%{kmod_name}
    ksrc=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}

    %{__install} -d ${RPM_BUILD_ROOT}/${INSTALL_MOD_DIR}
    %{__install} -m 755 _kmod_build_${kvariant}/src/libafs/MODLOAD-%{kversion}${kvariant}-MP/libafs.ko \
    ${RPM_BUILD_ROOT}/${INSTALL_MOD_DIR}/%{kmod_name}.ko
done

%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/

%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} \
    %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Set the module(s) to be executable, so that they will be 
# stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} a+x \{\} \;

# Remove the files that we do not require.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Wed Jul 24 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.5-1
- Update to 1.6.5
- CVE-2013-4134
- CVE-2013-4135

* Thu Jun 27 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.4-1
- Update to OpenAFS 1.6.4

* Mon Mar 04 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.2-1
- Bump to 1.6.2 to fix CVE-2013-1794 and CVE-2013-1795

* Mon May 07 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.1-1
- Update to OpenAFS 1.6.1 final

* Thu Sep 15 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-1
- Update to OpenAFS 1.6.0 final

* Fri Jul 29 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre7.1
- Backport kmod package to RHEL 5

* Mon Jul 25 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre7
- Update to OpenAFS 1.6.0 pre-release 7

* Wed Jun 08 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-0.pre6
- Update to OpenAFS 1.6.0 pre-release 6

* Fri Jan 07 2011 Jack Neely <jjneely@ncsu.edu> 0:1.4.14-1
- Build OpenAFS 1.4.14
- Update kmodtool to the latest EL6 kmodtool from ELRepo
- Update spec to conform better to ELRepo's EL6 kmodspec

* Fri Nov 12 2010 Jack Neely <jjneely@ncsu.edu> 0:1.4.12.1-6
- Rebuild for RHEL 6 final

* Wed Aug 05 2010 Jack Neely <jjneely@ncsu.edu> 0:1.4.12.1-5
- Initial build of openafs-kmod using ELRepo's kABI templates

