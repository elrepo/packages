# Define the kmod package name here.
%define kmod_name tpe
%define src_name tpe-lkm

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-123.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 1.0.3
Release: 990.git20140821%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     https://github.com/cormander/tpe-lkm

BuildRequires: perl
BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
# http://sourceforge.net/projects/tpe-lkm/files/latest/download
Source0: %{src_name}-%{version}.tar.gz
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
%setup -q -n %{src_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} %{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
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
* Thu Aug 21 2014 Philip J Perry <phil@elrepo.org> - 1.0.3-990.git20140821
- Initial el7 port of the kmod package.
- Update to latest git snapshot as a beta for 1.0.4 release.
