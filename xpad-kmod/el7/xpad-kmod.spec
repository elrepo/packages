# Define the kmod package name here.
%define kmod_name xpad

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-862.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 0.0.6
Release: 7%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.kernel.org/

BuildRequires: redhat-rpm-config, perl
ExclusiveArch: x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh

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
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Strip the modules(s).
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

# Sign the modules(s).
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
* Sat Jul 21 2018 Philip J Perry <phil@elrepo.org> - 0.0.6-7
- Rebuilt against RHEL 7.5 kernel
- Update to Kernel-4.14.56

* Mon Oct 02 2017 Philip J Perry <phil@elrepo.org> - 0.0.6-6
- Update to Kernel-4.12.14
- Rebuilt against RHEL 7.4 kernel [http://elrepo.org/bugs/view.php?id=789]

* Thu May 04 2017 Philip J Perry <phil@elrepo.org> - 0.0.6-5
- Update to Kernel-4.11
- Add support for Razer Wildcat gamepad

* Tue Mar 21 2017 Andrew Simpson <andrew.simpson@navy.mil> - 0.0.6-4
- Backported from Kernel-4.11-rc3
- Rebuilt against RHEL 7.3 kernel

* Thu Mar 05 2015 Philip J Perry <phil@elrepo.org> - 0.0.6-3
- Rebuilt against RHEL 7.1 kernel

* Wed Nov 19 2014 Philip J Perry <phil@elrepo.org> - 0.0.6-2
- Update to kernel-3.10.65
- Update Makefile.

* Fri Oct 17 2014 Alan Bartlett <ajb@elrepo.org> - 0.0.6-1
- Initial el7 build of the kmod package.
- Backported from the linux-3.10.58 long-term support sources.
