# Define the kmod package name here.
%define	 kmod_name rt3090sta

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.18-8.el5}

Name:	 %{kmod_name}-kmod
Version: 2.4.0.4
Release: 1%{?dist}
Group:	 System Environment/Kernel
License: GPLv2
Summary: Ralink %{kmod_name} driver module
URL:	 http://www.ralinktech.com.tw/data/drivers/20101216_RT3090_LinuxSTA_V2.4.0.4_WiFiBTCombo_DPO.tar.bz2

BuildRequires: redhat-rpm-config
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  20101216_RT3090_LinuxSTA_V2.4.0.4_WiFiBTCombo_DPO.tar.bz2
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
This package provides the %{kmod_name} kernel module(s) for the RaLink
RT3090 wireless 802.11n PCIe series network adapters.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T -a 0
pushd 20101216_RT3090_LinuxSTA_V2.4.0.4_WiFiBTCombo_DPO
%{__perl} -pi -e 's|(.*tftpboot$)|#$1|g' Makefile
popd
for kvariant in %{kvariants} ; do
    %{__cp} -a 20101216_RT3090_LinuxSTA_V2.4.0.4_WiFiBTCombo_DPO _kmod_build_$kvariant
done
%{__cp} -a %{SOURCE5} .
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
for kvariant in %{kvariants} ; do
    pushd _kmod_build_$kvariant
    %{__make} LINUX_SRC=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    popd
done

%install
%{__rm} -rf %{buildroot}
for kvariant in %{kvariants} ; do
    pushd _kmod_build_$kvariant
    %{__install} -d %{buildroot}/lib/modules/%{kversion}${kvariant}/extra/%{kmod_name}/
    %{__install} os/linux/rt3090sta.ko %{buildroot}/lib/modules/%{kversion}${kvariant}/extra/%{kmod_name}/
    %{__install} -d %{buildroot}%{_sysconfdir}/Wireless/RT2860STA/
    %{__install} RT2860STA.dat %{buildroot}%{_sysconfdir}/Wireless/RT2860STA/
    %{__install} RT2860STACard.dat %{buildroot}%{_sysconfdir}/Wireless/RT2860STA/
    %{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
    # Install docs
    %{__install} *.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
    %{__install} README_STA_pci %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
    popd
    %{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
    %{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
    %{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
done
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Mon May 02 2011 Philip J Perry <phil@elrepo.org> - 2.4.0.4-1.el5.elrepo
- Update to version 2.4.0.4.

* Tue Jul 13 2010 Philip J Perry <phil@elrepo.org> - 2.2.0.1-2.el5.elrepo
- Provide missing RT2860STA.dat [elrepo BugID #32]

* Sun Sep 06 2009 Philip J Perry <phil@elrepo.org> - 2.2.0.1-1.el5.elrepo
- Initial build of the kmod package.
- Patch makefile to build against kernel variants. [rt3090-makefile.patch]
