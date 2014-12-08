# Define the kmod package name here.
%define kmod_name smsc7500

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-71.el6.%{_target_cpu}}

Name: %{kmod_name}-kmod
Version: 1.04.10
Release: 1%{?dist}
Group: System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL: http://ubuntuone.com/p/153B/

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
# go figure why the name is lan7500 and not smsc7500
Source0: lan7500_%{version}.tar.gz
Source5: GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh
Source20: ELRepo-Makefile-%{kmod_name}

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

The module makes it possible to use a SMSC LAN7500 USB 2.0
to Ethernet 10/100/ 1000 Adapter with Vendor:Device ID Pairings of 0424:7500
Check the output of lsusb to see if your card reader is listed.

%prep
#%setup -q -n %{kmod_name}-%{version}
%setup -q -n %{kmod_name}
%{__rm} -f Makefile*
%{__cp} -a %{SOURCE20} Makefile
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
%{__make} KSRC=%{_usrsrc}/kernels/%{kversion}

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" modules_install M=$PWD
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} readme.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Nov 13 2014 Tru Huynh <tru@pasteur.fr> - 1.04.10-1
- Initial el6 build of the kmod package.
