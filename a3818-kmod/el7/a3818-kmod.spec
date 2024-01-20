# Define the kmod package name here.
%define kmod_name a3818

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1160.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 1.6.9
Release: 1%{?dist}
Group:   System Environment/Kernel
License: Other
Summary: %{kmod_name} kernel module(s)
URL:     http://www.caen.it

BuildRequires: redhat-rpm-config, perl
ExclusiveArch: x86_64

# Sources.
Source0:  A3818Drv-%{version}.tar.gz
Source5:  CAEN_License_Agreement.txt
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
%setup -q -n A3818Drv-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
%{__make} KERNELDIR=%{_usrsrc}/kernels/%{kversion}

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
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
* Fri Jan 19 2024 Akemi Yagi <toracat@elrepo.org> - 1.6.9-1
- Update version to 1.6.9 [https://elrepo.org/bugs/view.php?id=1420]

* Fri Jul 28 2023 Akemi Yagi <toracat@elrepo.org> - 1.6.8-1
- Updated to version 1.6.8 [https://elrepo.org/bugs/view.php?id=1376]

* Fri Jan 07 2022 Akemi Yagi <toracat@elrepo.org> - 1.6.7-1
- Updated to version 1.6.7 [https://elrepo.org/bugs/view.php?id=1182]
- Built against RHEL 7.9 kernel

* Fri Jan 25 2019 Akemi Yagi <toracat@elrepo.org> - 1.6.2-1
- Updated to version 1.6.2 [bug#895]
- Built against RHEL 7.6 kernel

* Fri Jul 28 2017 Akemi Yagi <toracat@elrepo.org> - 1.6.0-1
- Initial el7 build of the kmod package.
