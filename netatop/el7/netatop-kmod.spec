# Define the kmod package name here.
%define kmod_name netatop

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-123.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 0.3
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL: http://www.atoptool.nl

BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
Source0:  http://www.atoptool.nl/download/netatop-0.3.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh
Source20: ELRepo-Makefile-main
Source21: ELRepo-Makefile-module 
Source22: ELRepo-Makefile-daemon

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
Netatop gather statistics about the TCP and UDP packets that have been transmitted/received per process and per thread.
This module can be with the atop performance monitor.

%prep
%setup -q -n %{kmod_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
%{__rm} -f Makefile*
%{__rm} -f daemon/Makefile*
%{__rm} -f module/Makefile*
%{__cp} -a %{SOURCE20} Makefile
%{__cp} -a %{SOURCE21} module/Makefile
%{__cp} -a %{SOURCE22} daemon/Makefile

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
#%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
#%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
#%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
#%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
#%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
#%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;

%clean
%{__rm} -rf %{buildroot}

%changelog
* Mon Aug 4 2014 Rob Mokkink <rob@mokkinksystems.com> - 1.0.1
- Initial el7 build of netatop kernel module
