# Define the kmod package name here.
%define kmod_name mbgclock

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-696.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 20170425
Release: 0%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     https://www.meinbergglobal.com

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

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
%setup -q -n %{kmod_name}-%{version}
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
### %{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD
%{__make} -C $PWD %{?_smp_mflags} BUILD_DIR="${KSRC}"


%install
# Install udev rules for kmod device
%{__install} -Dp -m0644 mbgclock/55-mbgclock.rules %{buildroot}/etc/udev/rules.d/55-mbgclock.rules

## export INSTALL_MOD_PATH=%{buildroot}
## export INSTALL_MOD_DIR=extra/%{kmod_name}
## KSRC=%{_usrsrc}/kernels/%{kversion}
## %{__make} -C "${KSRC}" modules_install M=$PWD
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} mbgclock/*.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

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
* Thu May 18 2017 Akemi Yagi <toracat@elrepo.org> - 20170425
- Updated to mbgtools-lx-dev-2017-04-25 [http://elrepo.org/bugs/view.php?id=736]

* Tue Apr 18 2017 Akemi Yagi <toracat@elrepo.org> - 3.4.0
- Initial build for el6. [http://elrepo.org/bugs/view.php?id=726]
- -utils package provided by Pat Riehecky.
