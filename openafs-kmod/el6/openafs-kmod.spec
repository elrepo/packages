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

# Define the pre macro to build OpenAFS pre releases
# define pre pre1.1

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-44.1.el6.%{_target_cpu}}

Name:           %{kmod_name}-kmod
Version:        1.6.5
Release:        2%{?pre:%(echo .%{pre})}%{?dist}
Group:          System Environment/Kernel
License:        IBM
Summary:        %{kmod_name} kernel module(s)
URL:            http://www.openafs.org

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch:  i586 i686 x86_64 ppc ppc64
BuildRequires:  pam-devel, ncurses-devel, flex, byacc, bison, automake

# Sources.
Source0:  	    http://www.openafs.org/dl/openafs/%{version}/%{kmod_name}-%{version}%{?pre}-src.tar.bz2
Source5:	    LICENSE
Source10: 	    kmodtool-el6-%{kmod_name}.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}%{?pre}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" \
    > kmod-%{kmod_name}.conf

%build
ksrc=%{_usrsrc}/kernels/%{kversion}

%{configure} --with-afs-sysname=%{sysname} --enable-kernel-module \
    --disable-linux-syscall-probing  \
    --with-linux-kernel-headers="${ksrc}"
%{__make} MPS=MP only_libafs


%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=/lib/modules/%{kversion}/extra/%{kmod_name}
ksrc=%{_usrsrc}/kernels/%{kversion}

%{__install} -d ${RPM_BUILD_ROOT}/${INSTALL_MOD_DIR}
%{__install} -m 755 src/libafs/MODLOAD-%{kversion}-MP/libafs.ko \
    ${RPM_BUILD_ROOT}/${INSTALL_MOD_DIR}/%{kmod_name}.ko

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
* Sun Feb 09 2014 Jack Neely <jjneely@gmail.com> 0:1.6.5-2
- Build against RHEL 6.5 kernel 2.6.32-431.el6

* Wed Jul 24 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.5-1
- Update to 1.6.5
- CVE-2013-4134
- CVE-2013-4135

* Thu Jun 27 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.4-1
- Update to 1.6.4

* Mon Mar 04 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.2-1
- Bump to 1.6.2 to fix CVE-2013-1794 and CVE-2013-1795

* Thu Feb 21 2013 Jack Neely <jjneely@ncsu.edu> 0:1.6.1-5
- Rebuild with kernel 2.6.32-358.el6
- The binary kmods set Requires: kernel >= 2.6.32-358.el6 as they
  only work with those kernels

* Wed Aug 15 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.1-2
- Rebuild

* Tue Apr 03 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.1-1
- Update to 1.6.1 final

* Thu Jan 26 2012 Jack Neely <jjneely@ncsu.edu> 0:1.6.1-0.pre1.1
- Update to OpenAFS 1.6.1pre2 -- or what I think will be released
  as 1.6.1pre2

* Thu Sep 15 2011 Jack Neely <jjneely@ncsu.edu> 0:1.6.0-1
- Update to OpenAFS 1.6.0 final

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

