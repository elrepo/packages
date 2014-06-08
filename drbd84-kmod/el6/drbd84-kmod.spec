# $Id$
# Authority: dag

# Define the kmod package name here.
%define kmod_name drbd84
%define real_name drbd

### BEWARE: The kernel version is also mentioned in kmodtool !
# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion:%define kversion 2.6.32-358.el6.%{_target_cpu}}

Summary: Distributed Redundant Block Device driver for Linux
Name: %{kmod_name}-kmod
Version: 8.4.4
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

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
DRBD is a distributed replicated block device. It mirrors a
block device over the network to another machine. Think of it
as networked raid 1. It is a building block for setting up
high availability (HA) clusters.

%prep
%setup -n %{real_name}-%{version}

%configure \
    --with-km \
    --without-bashcompletion \
    --without-heartbeat \
    --without-pacemaker \
    --without-rgmanager \
    --without-udev \
    --without-utils \
    --without-xen

echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
ksrc="%{_usrsrc}/kernels/%{kversion}"
%{__make} %{?_smp_mflags} module KDIR="$ksrc"

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} drbd/*.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
for file in ChangeLog COPYING README; do
    %{__install} -Dp -m0644 $file %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/$file
done
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Sat Oct 12 2013 Philip J Perry <phil@elrepo.org> - 8.4.4-1
- Updated to release 8.4.4.

* Sat Jul 06 2013 Akemi Yagi <toracat@elrepo.org> - 8.4.3-1
- Updated to release 8.4.3.
- Built against RHEL-6.4.

* Thu Sep 06 2012 Dag Wieers <dag@wieers.com> - 8.4.2-1
- Updated to release 8.4.2.

* Tue Sep 04 2012 Dag Wieers <dag@wieers.com> - 8.4.1-3
- Rebuilt against RHEL-6.3.
  [http://elrepo.org/bugs/view.php?id=291]

* Tue Apr 17 2012 Philip J Perry <phil@elrepo.org> - 8.4.1-2
- Rebuilt against RHEL-6.2.
  [http://elrepo.org/bugs/view.php?id=210] and [http://elrepo.org/bugs/view.php?id=252]

* Wed Dec 21 2011 Dag Wieers <dag@wieers.com> - 8.4.1-1
- Updated to release 8.4.1.

* Fri Aug 12 2011 Dag Wieers <dag@wieers.com> - 8.4.0-2
- Conflicts with kmod-drbd82 and kmod-drbd83.

* Mon Aug 08 2011 Dag Wieers <dag@wieers.com> - 8.4.0-1
- Initial package for RHEL6.
