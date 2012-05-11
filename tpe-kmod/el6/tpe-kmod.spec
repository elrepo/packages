# Define the kmod package name here.
%define kmod_name tpe
%define src_name tpe-lkm

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-220.el6.%{_target_cpu}}

Name: %{kmod_name}-kmod
Version: 1.0.3
Release: 3%{?dist}
Group: System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL: https://github.com/cormander/tpe-lkm

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

#Sources.
# http://sourceforge.net/projects/tpe-lkm/files/latest/download
Source0: %{src_name}-%{version}.tar.gz
Source10: kmodtool-%{kmod_name}-el6.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

Trusted Path Execution (TPE) is a security feature that denies users from executing
programs that are not owned by root, or are writable. This closes the door on a
whole category of exploits where a malicious user tries to execute his or her
own code to hack the system.

%prep
%setup -q -n %{src_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}"  modules_install M=$PWD
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -p conf/tpe.modprobe.conf %{buildroot}%{_sysconfdir}/modprobe.d/tpe.conf
%{__install} -d %{buildroot}%{_sysconfdir}/sysconfig/modules/
%{__install} -p conf/tpe.sysconfig %{buildroot}%{_sysconfdir}/sysconfig/modules/tpe.modules
%{__install} -d %{buildroot}%{_sysconfdir}/sysctl.d/
%{__install} -p conf/tpe.sysctl %{buildroot}%{_sysconfdir}/sysctl.d/tpe.conf
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -p {FAQ,GPL,INSTALL,LICENSE,README} \
    %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

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

* Wed May 02 2012 Philip J Perry <phil@elrepo.org> - 1.0.2-1
- Update to version 1.0.2
- Install /etc/modprobe.d/tpe.conf
- Install /etc/sysconfig/modules/tpe.modules
- Install the docs

* Mon Apr 30 2012 Akemi Yagi <toracat@elrepo.org> - 1.0.1-1
- Initial rebuild for ELRepo.

* Wed Apr  4 2012 Corey Henderson <corman@cormander.com>
- Fixed NULL pointer in denied execution of long file paths

* Wed Jul  7 2011 Corey Henderson <corman@cormander.com>
- Initial build.
