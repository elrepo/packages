# Define the kmod package name here.
%define kmod_name nvidia-340xx

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-693.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 340.106
Release: 1.el7_4.elrepo
Group:   System Environment/Kernel
License: Proprietary
Summary: NVIDIA OpenGL kernel driver module
URL:	 http://www.nvidia.com/

BuildRequires: perl
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
Source0:  ftp://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
Source1:  blacklist-nouveau.conf
Source10: kmodtool-%{kmod_name}-el7.sh

# Patch0: legacy340.patch

NoSource: 0

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the proprietary NVIDIA OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override nvidia * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override nvidia-uvm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
sh %{SOURCE0} --extract-only --target nvidiapkg

# %patch0 -p1

%{__cp} -a nvidiapkg _kmod_build_

%build
export SYSSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/kernel
%{__make} module
popd
pushd _kmod_build_/kernel/uvm
%{__make} module
popd

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
pushd _kmod_build_/kernel
%{__install} nvidia.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
popd
pushd _kmod_build_/kernel/uvm
%{__install} nvidia-uvm.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
popd
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_prefix}/lib/modprobe.d/
%{__install} %{SOURCE1} %{buildroot}%{_prefix}/lib/modprobe.d/blacklist-nouveau.conf

# Sign the modules(s)
%if %{?_with_modsign:1}%{!?_with_modsign:0}
# If the module signing keys are not defined, define them here.
%{!?privkey: %define privkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.priv}
%{!?pubkey: %define pubkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.der}
for module in $(find %{buildroot} -type f -name \*.ko);
do %{__perl} /usr/src/kernels/%{kversion}/scripts/sign-file \
sha256 %{privkey} %{pubkey} $module;
done
%endif

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Feb 02 2018 Philip J Perry <phil@elrepo.org> - 340.106-1
- Updated to version 340.106

* Thu Aug 17 2017 Akemi Yagi <toracat@elrepo.org> - 340.102-4
- Patch to fix compilation issue applied
  [http://elrepo.org/bugs/view.php?id=768]

* Sat Aug 05 2017 Philip J Perry <phil@elrepo.org> - 340.102-3
- Rebuilt against RHEL 7.4 kernel

* Fri Mar 03 2017 Philip J Perry <phil@elrepo.org> - 340.102-2
- Rebuilt against kernel-3.10.0-514.10.2.el7 for kABI breakage

* Sat Feb 25 2017 Philip J Perry <phil@elrepo.org> - 340.102-1
- Updated to version 340.102

* Sat Dec 17 2016 Philip J Perry <phil@elrepo.org> - 340.101-1
- Updated to version 340.101

* Tue Nov 08 2016 Philip J Perry <phil@elrepo.org> - 340.98-1
- Updated to version 340.98
- Rebuilt against RHEL 7.3 kernel

* Fri Nov 20 2015 Philip J Perry <phil@elrepo.org> - 340.96-1
- Updated to version 340.96
- Rebuilt against RHEL 7.2 kernel

* Sat Sep 12 2015 Philip J Perry <phil@elrepo.org> - 340.93-1
- Updated to version 340.93

* Thu Mar 05 2015 Philip J Perry <phil@elrepo.org> - 340.76-2
- Rebuilt against RHEL 7.1 kernel

* Thu Feb 05 2015 Philip J Perry <phil@elrepo.org> - 340.76-1
- Update to version 340.76

* Tue Dec 16 2014 Philip J Perry <phil@elrepo.org> - 340.65-1
- Updated to version 340.65

* Fri Sep 26 2014 Philip J Perry <phil@elrepo.org> - 340.32-1
- Fork to legacy release nvidia-340xx

* Sat Aug 16 2014 Philip J Perry <phil@elrepo.org> - 340.32-1
- Updated to version 340.32

* Wed Jul 09 2014 Philip J Perry <phil@elrepo.org> - 340.24-1
- Updated to version 340.24
- Enabled Secure Boot

* Sat Jul 05 2014 Philip J Perry <phil@elrepo.org> - 331.89-1
- Updated to version 331.89

* Tue Jun 10 2014 Philip J Perry <phil@elrepo.org> - 331.79-2
- Rebuilt for rhel-7.0 release

* Wed May 21 2014 Philip J Perry <phil@elrepo.org> - 331.79-1
- Initial el7 build of the nvidia kmod package.
