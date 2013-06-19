# Define the kmod package name here.
%define kmod_name mvfs8
%define real_name mvfs

# Disable stripping
%define __os_install_post %{nil}

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.18-348.el5}

%define ratl_vendor_ver 509
%ifarch x86_64
%define ratl_compat32 -DRATL_COMPAT32
%else
%define ratl_compat32 %nil
%endif

Summary: Multi-version file system (MVFS), part of IBM Rational ClearCase
Name: %{kmod_name}-kmod
Version: 8.0.0.6
Release: 1%{?dist}
License: GPLv2
Group: System Environment/Kernel
URL: http://github.com/dagwieers/mvfs8/

BuildRequires: redhat-rpm-config
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)
ExclusiveArch: i686 x86_64

# Sources.
Source0:  https://github.com/dagwieers/mvfs8/archive/mvfs8-%{version}.tar.gz
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
This package provides the Multi-version file system (MVFS) which is
part of the IBM Rational ClearCase software.

For support, please visit http://www.ibm.com/software/support

%prep
%setup -c -T -a 0

%{__cat} <<EOF >%{kmod_name}-%{version}/mvfs_param.mk.config
CONFIG_MVFS=m
RATL_EXTRAFLAGS := -DRATL_REDHAT -DRATL_VENDOR_VER=%{ratl_vendor_ver} -DRATL_EXTRA_VER=0 %{ratl_compat32}
LINUX_KERNEL_DIR=/lib/modules/%{kversion}/build
EOF

for kvariant in %{kvariants} ; do
    %{__cp} -a %{kmod_name}-%{version}/ _kmod_build_$kvariant
done
%{__cp} -a %{SOURCE5} .
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override %{real_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
for kvariant in %{kvariants} ; do
    KSRC=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    %{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD/_kmod_build_$kvariant
done

%install
%{__rm} -rf %{buildroot}
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=kernel/fs/%{real_name}
for kvariant in %{kvariants} ; do
    KSRC=%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}
    %{__make} -C "${KSRC}" modules_install M=$PWD/_kmod_build_$kvariant
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{kmod_name}-%{version}/README.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Wed Jun 19 2013 Dag Wieers <dag@wieers.com> - 8.0.0.6-1
- Initial el5 build of the kmod package.
