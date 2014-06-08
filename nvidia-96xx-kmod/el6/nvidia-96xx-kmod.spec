# Define the kmod package name here.
%define	 kmod_name nvidia-96xx

# If kversion isn't defined on the rpmbuild line, define it here.
# Lets build against the latest release as RHEL6_3 has kABI issues
# https://access.redhat.com/knowledge/solutions/220223
# Fixed in kernel 2.6.32-279.11.1.el6 onwards
%{!?kversion: %define kversion 2.6.32-279.22.1.el6.%{_target_cpu}}

Name:	 %{kmod_name}-kmod
Version: 96.43.23
Release: 1%{?dist}
Group:	 System Environment/Kernel
License: Proprietary
Summary: NVIDIA 96xx OpenGL kernel driver module
URL:	 http://www.nvidia.com/

BuildRequires:	redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0: ftp://download.nvidia.com/XFree86/Linux-x86/%{version}/NVIDIA-Linux-x86-%{version}-pkg0.run
Source1: ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}-pkg2.run
Source10: kmodtool-%{kmod_name}-el6.sh

NoSource: 0
NoSource: 1

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define	debug_package %{nil}

%description
This package provides the proprietary NVIDIA 96xx OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%ifarch i686
sh %{SOURCE0} --extract-only --target nvidiapkg
%endif

%ifarch x86_64
sh %{SOURCE1} --extract-only --target nvidiapkg
%endif

%{__cp} -a nvidiapkg _kmod_build_

%build
export SYSSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/usr/src/nv
%{__make} module
popd

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
pushd _kmod_build_/usr/src/nv
ksrc=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${ksrc}" modules_install M=$PWD
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Tue Feb 19 2013 Philip J Perry <phil@elrepo.org> - 96.43.23-1.el6.elrepo
- Update to version 96.43.23.

* Sat Dec 10 2011 Philip J Perry <phil@elrepo.org> - 96.43.20-1.el6.elrepo
- Update to version 96.43.20.

* Fri Feb 04 2011 Philip J Perry <phil@elrepo.org> - 96.43.19-1.el6.elrepo
- Fork to el6
- Update to version 96.43.19.

* Sat Aug 21 2010 Philip J Perry <phil@elrepo.org> - 96.43.18-1.el5.elrepo
- Update to version 96.43.18.

* Thu Feb 04 2010 Philip J Perry <phil@elrepo.org> - 96.43.16-1.el5.elrepo
- Update to version 96.43.16.
- Removed Requires kernel > 2.6.18-128.el5 [BugID 33]

* Tue Dec 15 2009 Akemi Yagi <toracat@elrepo.org> - 96.43.14-1.el5.elrepo
- Update to version 96.43.14.
- Nvidia legacy 96xx driver rebuilt for ELRepo, based on package contributed by Marco Giunta.
