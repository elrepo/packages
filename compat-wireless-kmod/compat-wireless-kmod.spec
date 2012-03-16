# Define the kmod package name here.
%define kmod_name compat-wireless

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-220.el6.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 3.3
Release: 0.2.rc6.1%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://linuxwireless.org/en/users/Download/stable

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0:  http://www.orbit-lab.org/kernel/compat-wireless-3-stable/v3.3/compat-wireless-3.3-rc6-1.tar.bz2
Source10: kmodtool-%{kmod_name}-el6.sh

# Patches.
Patch0: compat-wireless-remove-olpc_ec_wakeup_calls.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}-rc6-1
%patch0 -p0
echo "blacklist iwlagn" > blacklist-compat-wireless.conf
echo "blacklist ar9170usb" >> blacklist-compat-wireless.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} %{?_smp_mflags} KLIB=/lib/modules/%{kversion}

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
# Install the modules
find . -type f -name \*.ko -exec %{__cp} -a \{\} %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/ \;
# Automatically generate the overrides file
MODULES_DIR=%{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}
:>kmod-%{kmod_name}.conf
for FILENAME in $MODULES_DIR/*.ko
do
    MODULE="$( echo $FILENAME | sed 's/.*\/\(.*\)\.ko/\1/' )"
    echo "override $MODULE * weak-updates/%{kmod_name}" >> kmod-%{kmod_name}.conf
done
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} blacklist-compat-wireless.conf %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} COPYRIGHT README %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Mar 16 2012 Philip J Perry <phil@elrepo.org> - 3.3-0.2.rc6.1
- Fix build issue on i686 [compat-wireless-remove-olpc_ec_wakeup_calls.patch]
- Add iwlagn and ar9170usb to the blacklist.

* Thu Mar 15 2012 Philip J Perry <phil@elrepo.org> - 3.3-0.1.rc6.1
- Initial el6 build of the kmod package.
