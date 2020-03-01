# Define the kmod package name here.
%define kmod_name r8168

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-754.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 8.048.00
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.realtek.com.tw/

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.bz2
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh
Source20: ELRepo-Makefile-%{kmod_name}

# Patches.
Patch0: ELRepo-r8168-8.046.00.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s) for Realtek RTL8168/RTL8111,
RTL8168B/RTL8111B, RTL8168C/RTL8111C, RTL8168D/RTL8111D, RTL8168E/RTL8111E
and RTL8168F/RTL8111F Gigabit Ethernet controllers with the PCI-Express interface.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}
%patch0 -p1
%{__rm} -f src/Makefile*
%{__cp} -a %{SOURCE20} src/Makefile
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "blacklist r8169" > blacklist-r8169.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD/src

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" modules_install M=$PWD/src
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} blacklist-r8169.conf %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Sun Mar 01 2020 Philip J Perry <phil@elrepo.org> - 8.048.00-1
- Updated to version 8.048.00

* Sat Aug 10 2019 Philip J Perry <phil@elrepo.org> - 8.047.04-1
- Updated to version 8.047.04

* Thu Sep 06 2018 Alan Bartlett <ajb@elrepo.org> - 8.046.00-1
- Updated to version 8.046.00

* Tue Nov 14 2017 Philip J Perry <phil@elrepo.org> - 8.045.08-1
- Updated to version 8.045.08

* Mon Oct 17 2016 Alan Bartlett <ajb@elrepo.org> - 8.043.01-1
- Updated to version 8.043.01

* Mon Jul 27 2015 Alan Bartlett <ajb@elrepo.org> - 8.040.00-1
- Updated to version 8.040.00

* Wed Oct 08 2014 Alan Bartlett <ajb@elrepo.org> - 8.039.00-1
- Updated to version 8.039.00

* Thu May 01 2014 Alan Bartlett <ajb@elrepo.org> - 8.038.00-1
- Updated to version 8.038.00

* Thu Oct 10 2013 Alan Bartlett <ajb@elrepo.org> - 8.037.00-2
- Corrected the corrupted specification file.
- [http://elrepo.org/bugs/view.php?id=414]

* Wed Oct 09 2013 Alan Bartlett <ajb@elrepo.org> - 8.037.00-1
- Updated to version 8.037.00

* Mon Jul 01 2013 Alan Bartlett <ajb@elrepo.org> - 8.036.00-1
- Updated to version 8.036.00

* Sat Jan 05 2013 Alan Bartlett <ajb@elrepo.org> - 8.035.00-1
- Updated to version 8.035.00

* Mon Sep 17 2012 Alan Bartlett <ajb@elrepo.org> - 8.032.00-1
- Updated to version 8.032.00

* Wed May 30 2012 Alan Bartlett <ajb@elrepo.org> - 8.031.00-1
- Updated to version 8.031.00

* Sat May 19 2012 Akemi Yagi <toracat@elrepo.org> - 8.030.00-1
- Updated to version 8.030.00

* Mon Apr 09 2012 Alan Bartlett <ajb@elrepo.org> - 8.029.00-1
- Updated to version 8.029.00

* Wed Mar 14 2012 Alan Bartlett <ajb@elrepo.org> - 8.028.00-2
- Reinstated the blacklisting of the r8169 module.

* Thu Feb 09 2012 Alan Bartlett <ajb@elrepo.org> - 8.028.00-1
- Updated to version 8.028.00

* Fri Dec 30 2011 Alan Bartlett <ajb@elrepo.org> - 8.027.00-1
- Updated to version 8.027.00
- Removed the blacklisting of the r8169 module.

* Mon Aug 29 2011 Alan Bartlett <ajb@elrepo.org> - 8.025.00-1
- Updated to version 8.025.00

* Mon May 29 2011 Alan Bartlett <ajb@elrepo.org> - 8.024.00-1
- Updated to version 8.024.00

* Mon Apr 25 2011 Alan Bartlett <ajb@elrepo.org> - 8.023.00-1
- Updated to version 8.023.00

* Sun Mar 20 2011 Alan Bartlett <ajb@elrepo.org> - 8.022.00-1
- Updated to version 8.022.00

* Wed Feb 09 2011 Alan Bartlett <ajb@elrepo.org> - 8.021.00-1
- Updated to version 8.021.00

* Fri Dec 10 2010 Alan Bartlett <ajb@elrepo.org> - 8.020.00-1
- Updated to version 8.020.00

* Sat Dec 04 2010 Alan Bartlett <ajb@elrepo.org> - 8.019.00-1
- Initial el6 build of the kmod package.
