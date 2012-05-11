# Define the kmod package name here.
%define kmod_name tpe
%define src_name tpe-lkm

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.18-308.el5}

Name:    %{kmod_name}-kmod
Version: 1.0.3
Release: 3%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     https://github.com/cormander/tpe-lkm

BuildRequires: redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
# http://sourceforge.net/projects/tpe-lkm/files/latest/download
Source0:  %{src_name}-%{version}.tar.gz
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

Trusted Path Execution (TPE) is a security feature that denies users from executing
programs that are not owned by root, or are writable. This closes the door on a
whole category of exploits where a malicious user tries to execute his or her
own code to hack the system.

%prep
%setup -q -c -T -a 0
for kvariant in %{kvariants} ; do
    %{__cp} -a %{src_name}-%{version} _kmod_build_$kvariant
done
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
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -p _kmod_build_$kvariant/conf/tpe.modprobe.conf \
    %{buildroot}%{_sysconfdir}/modprobe.d/tpe.conf
%{__install} -d %{buildroot}%{_sysconfdir}/sysconfig/modules/
%{__install} -p _kmod_build_$kvariant/conf/tpe.sysconfig \
    %{buildroot}%{_sysconfdir}/sysconfig/modules/tpe.modules
%{__install} -d %{buildroot}%{_sysconfdir}/sysctl.d/
%{__install} -p _kmod_build_$kvariant/conf/tpe.sysctl \
    %{buildroot}%{_sysconfdir}/sysctl.d/tpe.conf
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -p _kmod_build_$kvariant/{FAQ,GPL,INSTALL,LICENSE,README} \
    %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu May 11 2012 Philip J Perry <phil@elrepo.org> - 1.0.3-3
- Fix file permissions.

* Thu May 10 2012 Philip J Perry <phil@elrepo.org> - 1.0.3-2
- Fix typo: Install /etc/sysctl.d/tpe.conf

* Thu May 10 2012 Philip J Perry <phil@elrepo.org> - 1.0.3-1
- Update to version 1.0.3
- Install /etc/sysctl/tpe.conf

* Thu May 03 2012 Philip J Perry <phil@elrepo.org> - 1.0.2-2
- Fix PAE build [tpe-1.0.2-PAE-hijacks.el5.patch]

* Thu May 03 2012 Philip J Perry <phil@elrepo.org> - 1.0.2-1
- Initial el5 build of the kmod package.
- Disable PAE build which fails for kernel-PAE-2.6.18-308.el5
- Install /etc/modprobe.d/tpe.conf
- Install /etc/sysconfig/modules/tpe.modules
- Install the docs
