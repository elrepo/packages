# Define the kmod package name here.
%define kmod_name wacom
%define source_name input-%{kmod_name}

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-693.el7.%{_target_cpu}}

Name:           %{kmod_name}-kmod
Version:        0.37.1
Release:        1.el7_4.elrepo
Group:          System Environment/Kernel
License:        GPLv2
Summary:        %{kmod_name} kernel module(s)
URL:            https://github.com/linuxwacom/input-wacom
# URL:            https://downloads.sourceforge.net/linuxwacom/

BuildRequires:  perl
BuildRequires:  redhat-rpm-config
ExclusiveArch:  x86_64

# Sources.
Source0:        http://downloads.sourceforge.net/%{source_name}/%{source_name}-%{version}.tar.bz2
Source5:        GPL-v2.0.txt
Source10:       kmodtool-%{kmod_name}-el7.sh

# Magic hidden here.
%{expand:%( sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "" )}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{source_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "override %{kmod_name}_w8001 * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
echo "override hid-%{kmod_name} * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%configure --with-kernel-version=%{kversion} --with-kernel=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M="${PWD}"
pushd 3.7 >/dev/null
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M="${PWD}"
popd >/dev/null
pushd 3.17 >/dev/null
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M="${PWD}"
popd >/dev/null

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} 3.7/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} 3.17/%{kmod_name}_w8001.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} 3.17/hid-%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} AUTHORS %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} ChangeLog %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} COPYING %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} NEWS %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} version %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# strip the modules(s)
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

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
* Fri Nov 03 2017 Philip J Perry <phil@elrepo.org> - 0.37.1-1
- Update to version 0.37.1

* Wed Oct 04 2017 Philip J Perry <phil@elrepo.org> - 0.37.0-2
- Include hid-wacom module.

* Wed Oct 04 2017 Philip J Perry <phil@elrepo.org> - 0.37.0-1
- Update to 0.37.0
- Rebuilt against RHEL 7.4 kernel [http://elrepo.org/bugs/view.php?id=791]

* Tue Apr 18 2017 Tomasz Tomasik <scx.mail@gmail.com> - 0.35.0-1
- Update to 0.35.0
- Rebuilt for 3.10.0-514 kernel
- Cleanup SPEC file

* Thu Mar 24 2016 Tomasz Tomasik <scx.mail@gmail.com> - 0.30.2-1
- Initial el7 build of the kmod package.
  [https://sourceforge.net/projects/linuxwacom/files/xf86-input-wacom/]

