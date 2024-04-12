# Define upstream file name here.
%define upstream_name mbgtools-lx

# Define the kmod package name here.
%define kmod_name mbgclock

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-1160.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 4.2.26
Release: 1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     https://www.meinbergglobal.com

BuildRequires: redhat-rpm-config
ExclusiveArch: x86_64

# Sources.
Source0:  %{upstream_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh

# Patches.

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%package -n %{kmod_name}-utils
Summary: Userspace utilities for %{kmod_name}
Group: System Environment/Kernel

%description -n %{kmod_name}-utils
Userspace utilities for %{kmod_name}

%prep
%setup -q -n %{upstream_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
## %%{__make} -C "${KSRC}" %%{?_smp_mflags} modules M=$PWD
%{__make} -C $PWD %{?_smp_mflags} BUILD_DIR="${KSRC}"

%install
# Install udev rules for kmod device
%{__install} -Dp -m0644 udev/55-mbgclock.rules %{buildroot}/etc/udev/rules.d/55-mbgclock.rules

%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} mbgclock/*.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Strip the modules(s).
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

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

# Quick and dirty loop for mbg utils
for binary in $(ls); do
    if [[ -x ${binary}/${binary} ]]; then
        %{__install} -Dp -m0755 ${binary}/${binary} %{buildroot}%{_sbindir}/${binary}
    fi
done

%clean
%{__rm} -rf %{buildroot}

%files -n %{kmod_name}-utils
%defattr(0644,root,root,-)
%attr(0755,root,root) %{_sbindir}/mbg*
/etc/udev/rules.d/55-mbgclock.rules

%changelog
* Fri Apr 12 2024 Tuan Hoang <tqhoang@elrepo.org> - 4.2.26-1
- Updated to version 4.2.26

* Fri Apr 24 2020 Akemi Yagi <toracat@elrepo.org> - 4.2.10-1
- Initial build for el7 [http://elrepo.org/bugs/view.php?id=1002]
