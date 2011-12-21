# $Id$
# Authority: dag

# Define the kmod package name here.
%define kmod_name drbd84
%define real_name drbd

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion:%define kversion 2.6.18-128.el5}

Summary: Distributed Redundant Block Device driver for Linux
Name: %{kmod_name}-kmod
Version: 8.4.1
Release: 1%{?dist}
License: GPL
Group: System Environment/Kernel
URL: http://www.drbd.org/

# Sources.
Source0: http://oss.linbit.com/drbd/8.4/drbd-%{version}.tar.gz
Source10: kmodtool-%{kmod_name}.sh
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)

ExclusiveArch: i686 x86_64
BuildRequires: redhat-rpm-config

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
DRBD is a distributed replicated block device. It mirrors a
block device over the network to another machine. Think of it
as networked raid 1. It is a building block for setting up
high availability (HA) clusters.

%prep
%setup -c -T -a 0
pushd %{real_name}-%{version}
%configure \
    --with-km \
    --without-bashcompletion \
    --without-heartbeat \
    --without-pacemaker \
    --without-rgmanager \
    --without-udev \
    --without-utils \
    --without-xen
popd

for kvariant in %{kvariants} ; do
    %{__cp} -a %{real_name}-%{version} _kmod_build_$kvariant
done
echo "/usr/lib/rpm/redhat/find-requires | %{__sed} -e '/^ksym.*/d'" > filter-requires.sh
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
for kvariant in %{kvariants} ; do
    ksrc="%{_usrsrc}/kernels/%{kversion}${kvariant:+-$kvariant}-%{_target_cpu}"
    kpath="$PWD/_kmod_build_$kvariant"
    %{__make} -C "$kpath" module KDIR="$ksrc"
done

%install
%{__rm} -rf %{buildroot}
for kvariant in %{kvariants} ; do
    %{__install} -d %{buildroot}/lib/modules/%{kversion}${kvariant}/extra/%{kmod_name}
    %{__install} _kmod_build_$kvariant/drbd/*.ko \
        %{buildroot}/lib/modules/%{kversion}${kvariant}/extra/%{kmod_name}/
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
pushd %{real_name}-%{version}
  for file in ChangeLog COPYING README; do
    %{__install} -Dp -m0644 $file %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/$file
  done
popd
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Wed Dec 21 2011 Dag Wieers <dag@wieers.com> - 8.4.1-1
- Updated to release 8.4.1.

* Fri Aug 12 2011 Dag Wieers <dag@wieers.com> - 8.4.0-2
- Conflicts with kmod-drbd82 and kmod-drbd83.

* Mon Aug 08 2011 Dag Wieers <dag@wieers.com> - 8.4.0-1
- Initial package for RHEL5.
