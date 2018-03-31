# HighPoint tarball used --
# rr174x-linux-src-v2.4-091009-1434.tar.gz

# Define the kmod package name here.
%define kmod_name rr174x

# Define the bundle name and source creation date-time here.
%define  bundle_name rr174x-linux-src
%define  scdt 091009-1434

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-696.23.1.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 2.4
Release: 2.el6_9.elrepo
Group:   System Environment/Kernel
License: Open Source but Proprietary
Summary: %{kmod_name} kernel module(s)
URL:     http://www.highpoint-tech.com/

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{bundle_name}-v%{version}-%{scdt}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh
Source15: ELRepo-%{kmod_name}-modules

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the HighPoint RocketRAID %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{bundle_name}-v%{version}
%{__cp} -a %{SOURCE5} .
%{__cp} -a %{SOURCE15} %{kmod_name}.modules
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "blacklist sata_mv" > blacklist-sata_mv.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
pushd product/rr1740pm/linux >/dev/null
%{__make} KERNELDIR="${KSRC}" %{?_smp_mflags}
popd >/dev/null

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} product/rr1740pm/linux/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}/etc/modprobe.d/
%{__install} blacklist-sata_mv.conf %{buildroot}/etc/modprobe.d/
%{__install} -d %{buildroot}/etc/sysconfig/modules/
%{__install} %{kmod_name}.modules %{buildroot}/etc/sysconfig/modules/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Mon Mar 19 2018 Philip J Perry <phil@elrepo.org> - 2.4-2
- Built against latest kernel for retpoline supported

* Wed Sep 28 2011 Alan Bartlett <ajb@elrepo.org> - 2.4-1
- Initial el6 build of the kmod package.
