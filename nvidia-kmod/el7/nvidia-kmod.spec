# Define the kmod package name here.
%define kmod_name nvidia

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1160.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 550.144.03
Release: 1%{?dist}
Group:   System Environment/Kernel
License: Proprietary
Summary: NVIDIA OpenGL kernel driver module
URL:	 https://www.nvidia.com/

BuildRequires: perl
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
Source0:  https://download.nvidia.com/XFree86/Linux-x86_64/%{version}/NVIDIA-Linux-x86_64-%{version}.run
Source1:  blacklist-nouveau.conf
Source10: kmodtool-%{kmod_name}-el7.sh
Source15: nvidia-provides.sh

%if %{?_with_src:0}%{!?_with_src:1}
NoSource: 0
%endif

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

# Define for nvidia-provides
%define __find_provides %{SOURCE15}

%description
This package provides the proprietary NVIDIA OpenGL kernel driver module.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -c -T
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override %{kmod_name}-drm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_name}-modeset * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_name}-peermem * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override %{kmod_name}-uvm * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
sh %{SOURCE0} --extract-only --target nvidiapkg
%{__cp} -a nvidiapkg _kmod_build_

%build
export SYSSRC=%{_usrsrc}/kernels/%{kversion}
pushd _kmod_build_/kernel
%{__make} %{?_smp_mflags} module
popd

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
pushd _kmod_build_/kernel
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-drm.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-modeset.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-peermem.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}-uvm.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
popd
pushd _kmod_build_
# Install GPU System Processor (GSP) firmware
%{__install} -d %{buildroot}/lib/firmware/nvidia/%{version}/
%{__install} -p -m 0755 firmware/gsp_ga10x.bin %{buildroot}/lib/firmware/nvidia/%{version}/gsp_ga10x.bin
%{__install} -p -m 0755 firmware/gsp_tu10x.bin %{buildroot}/lib/firmware/nvidia/%{version}/gsp_tu10x.bin
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
* Tue Jan 28 2025 Tuan Hoang <tqhoang@elrepo.org> - 550.144.03-1
- Updated to version 550.144.03

* Thu Dec 26 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.142-1
- Updated to version 550.142

* Tue Nov 19 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.135-1
- Updated to version 550.135

* Tue Oct 22 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.127.05-1
- Updated to version 550.127.05

* Sat Oct 05 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.120-1
- Updated to version 550.120

* Thu Aug 01 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.107.02-1
- Updated to version 550.107.02

* Tue Jul 09 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.100-1
- Updated to version 550.100

* Wed Jun 05 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.90.07-1
- Updated to version 550.90.07

* Tue May 14 2024 Tuan Hoang <tqhoang@elrepo.org> - 550.78-1
- Updated to version 550.78

* Thu Apr 18 2024 Philip J Perry <phil@elrepo.org> - 550.76-1
- Updated to version 550.76

* Sat Mar 23 2024 Philip J Perry <phil@elrepo.org> - 550.67-1
- Updated to version 550.67

* Sun Feb 25 2024 Philip J Perry <phil@elrepo.org> - 550.54.14-1
- Updated to version 550.54.14

* Wed Jan 17 2024 Tuan Hoang <tqhoang@elrepo.org> - 535.154.05-1
- Updated to version 535.154.05

* Wed Nov 08 2023 Philip J Perry <phil@elrepo.org> - 535.129.03-1
- Updated to version 535.129.03

* Mon Sep 25 2023 Philip J Perry <phil@elrepo.org> - 535.113.01-1
- Updated to version 535.113.01

* Wed Aug 23 2023 Philip J Perry <phil@elrepo.org> - 535.104.05-1
- Updated to version 535.104.05

* Wed Aug 09 2023 Philip J Perry <phil@elrepo.org> - 535.98-1
- Updated to version 535.98

* Thu Jul 20 2023 Philip J Perry <phil@elrepo.org> - 535.86.05-1
- Updated to version 535.86.05

* Sun Jun 25 2023 Philip J Perry <phil@elrepo.org> - 535.54.03-1
- Updated to version 535.54.03

* Wed May 10 2023 Philip J Perry <phil@elrepo.org> - 525.116.04-1
- Updated to version 525.116.04

* Wed Apr 26 2023 Philip J Perry <phil@elrepo.org> - 525.116.03-1
- Updated to version 525.116.03

* Fri Mar 31 2023 Philip J Perry <phil@elrepo.org> - 525.105.17-1
- Updated to version 525.105.17

* Thu Mar 30 2023 Philip J Perry <phil@elrepo.org> - 525.89.02-1
- Updated to version 525.89.02

* Sat Jan 21 2023 Philip J Perry <phil@elrepo.org> - 525.85.05-1
- Updated to version 525.85.05

* Fri Jan 06 2023 Philip J Perry <phil@elrepo.org> - 525.78.01-1
- Updated to version 525.78.01

* Tue Nov 29 2022 Philip J Perry <phil@elrepo.org> - 525.60.11-1
- Updated to version 525.60.11

* Sun Nov 27 2022 Philip J Perry <phil@elrepo.org> - 515.86.01-1
- Updated to version 515.86.01

* Sat Sep 24 2022 Philip J Perry <phil@elrepo.org> - 515.76-1
- Updated to version 515.76

* Sun Aug 07 2022 Philip J Perry <phil@elrepo.org> - 515.65.01-1
- Updated to version 515.65.01

* Wed Jun 29 2022 Philip J Perry <phil@elrepo.org> - 515.57-1
- Updated to version 515.57

* Fri Jun 03 2022 Philip J Perry <phil@elrepo.org> - 515.48.07-1
- Updated to version 515.48.07

* Mon May 23 2022 Philip J Perry <phil@elrepo.org> - 510.73.05-1
- Updated to version 510.73.05

* Wed Apr 27 2022 Philip J Perry <phil@elrepo.org> - 510.68.02-1
- Updated to version 510.68.02

* Sat Mar 26 2022 Philip J Perry <phil@elrepo.org> - 510.60.02-1
- Updated to version 510.60.02

* Tue Feb 15 2022 Philip J Perry <phil@elrepo.org> - 510.54-1
- Updated to version 510.54

* Thu Feb 03 2022 Philip J Perry <phil@elrepo.org> - 510.47.03-1
- Updated to version 510.47.03

* Tue Feb 01 2022 Philip J Perry <phil@elrepo.org> - 470.103.01-1
- Updated to version 470.103.01

* Tue Dec 14 2021 Philip J Perry <phil@elrepo.org> - 470.94-1
- Updated to version 470.94

* Thu Nov 11 2021 Philip J Perry <phil@elrepo.org> - 470.86-1
- Updated to version 470.86

* Thu Oct 28 2021 Philip J Perry <phil@elrepo.org> - 470.82.00-1
- Updated to version 470.82.00

* Tue Sep 21 2021 Philip J Perry <phil@elrepo.org> - 470.74-1
- Updated to version 470.74

* Wed Aug 11 2021 Philip J Perry <phil@elrepo.org> - 470.63.01-1
- Updated to version 470.63.01
- Add firmware for nvidia.ko module

* Mon Jul 19 2021 Philip J Perry <phil@elrepo.org> - 470.57.02-1
- Updated to version 470.57.02
- Adds nvidia-peermem kernel module
