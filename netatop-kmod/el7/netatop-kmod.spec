# Define the kmod package name here.
%define kmod_name netatop

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-123.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 0.3
Release: 2%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.atoptool.nl/

BuildRequires: redhat-rpm-config,perl, zlib-devel
ExclusiveArch: x86_64

# Sources.
Source0:  http://www.atoptool.nl/download/%{kmod_name}-%{version}.tar.gz
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
Netatop gather statistics about the TCP and UDP packets that have been
transmitted/received per process and per thread and can be used with the
atop performance monitor.

%prep
%setup -q -n %{kmod_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
pushd man
for M in *; do gzip $M; done
popd

%build
pushd module
%{__make} KERNDIR=%{_usrsrc}/kernels/%{kversion}
popd
pushd daemon
%{__make} KERNDIR=%{_usrsrc}/kernels/%{kversion}
popd

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} module/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -d %{buildroot}%{_mandir}/man4/
%{__install} man/*.4.gz %{buildroot}%{_mandir}/man4/
%{__install} -d %{buildroot}%{_mandir}/man8/
%{__install} man/*.8.gz %{buildroot}%{_mandir}/man8/

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
* Mon Aug 4 2014 Alan Bartlett <ajb@elrepo.org> - 0.3-2
- Update this specification file to the current standard.

* Mon Aug 4 2014 Rob Mokkink <rob@mokkinksystems.com> - 0.3-1
- Initial el7 build of netatop kernel module
