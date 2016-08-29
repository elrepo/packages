# Define the kmod package name here.
%define kmod_name alx

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-504.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 0.0
Release: 10%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.kernel.org/

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.bz2
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" modules_install M=$PWD
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Mon Aug 29 2016 Philip J Perry <phil@elrepo.org> - 0.0-10
- Backported from kernel-3.10.103
- Work around the DMA RX overflow issue [2016-08-27]

* Wed Mar 04 2015 Philip J Perry <phil@elrepo.org> - 0.0-9
- Backported from kernel-3.10.70
- Fix alx_poll() [2015-01-27]
- Fix missing __CHECK_ENDIAN__ define

* Mon Nov 04 2013 Philip J Perry <phil@elrepo.org> - 0.0-8
- Fix multicast stream (patch submitted by aroguez)
  [http://elrepo.org/bugs/view.php?id=422]

* Mon Jul 29 2013 Philip J Perry <phil@elrepo.org> - 0.0-7
- Fix lockdep annotation [2013-07-28]

* Wed Jul 17 2013 Philip J Perry <phil@elrepo.org> - 0.0-6
- Rebase driver to upstream kernel code.
- Backported from kernel-3.10.1

* Sun Jul 14 2013 Alan Bartlett <ajb@elrepo.org> - 0.0-5
- Removed previously added filter.
- Renamed pcmcia_loop_tuple() as compat_pcmcia_loop_tuple()
  [http://elrepo.org/bugs/view.php?id=388]

* Sat Jul 13 2013 Alan Bartlett <ajb@elrepo.org> - 0.0-4
- Added a filter to the provides.
  [http://elrepo.org/bugs/view.php?id=388]

* Mon Mar 11 2013 Paul Hampson <Paul.Hampson@Pobox.com> - 0.0-3
- Update to compat-drivers release 2013-03-07-u with RHEL6_4-specific patches.
  [http://elrepo.org/bugs/view.php?id=361]

* Sat Dec 22 2012 Philip J Perry <phil@elrepo.org> - 0.0-2
- Add missing modules
  [http://elrepo.org/bugs/view.php?id=306]

* Mon Oct 15 2012 Philip J Perry <phil@elrepo.org> - 0.0-1
- Initial el6 build of the kmod package from nightly snapshot 2012-10-03-pc.
  [http://elrepo.org/bugs/view.php?id=306]
