# Define the kmod package name here.
%define kmod_name bbswitch

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-71.el6.%{_target_cpu}}

Name: %{kmod_name}-kmod
Version: 0.4.1
Release: 1%{?dist}
Group: System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL: https://github.com/Bumblebee-Project/bbswitch

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0: %{kmod_name}-%{version}.tar.gz
Source1: bbswitch.conf
Source5: GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
bbswitch is a kernel module which automatically detects the required ACPI calls for two kinds of Optimus laptops. It has been verified to work with "real" Optimus and "legacy" Optimus laptops (at least, that is how I call them). The machines on which these tests has performed are:
    Clevo B7130 - GT 425M ("real" Optimus, Lekensteyns laptop)
    Dell Vostro 3500 - GT 310M ("legacy" Optimus, Samsagax' laptop)
(note: there is no need to add more supported laptops here as the universal calls should work for every laptop model supporting either Optimus calls)

It's preferred over manually hacking with the acpi_call module because it can detect the correct handle preceding _DSM and has some built-in safeguards:

You're not allowed to disable a card if a driver (nouveau, nvidia) is loaded.
Before suspend, the card is automatically enabled. When resuming, it's disabled again if that was the case before suspending. Hibernation should work, but it not tested.

%prep
%setup -q -n %{kmod_name}-%{version}
%{__cp} -a %{SOURCE5} .
%{__cp} -a %{SOURCE1} .
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD

%install
export INSTALL_MOD_PATH=%{buildroot}
export INSTALL_MOD_DIR=extra/%{kmod_name}
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" modules_install M=$PWD
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
mkdir -p %{buildroot}/etc/modprobe.d
%{__install} bbswitch.conf %{buildroot}/etc/modprobe.d/
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Thu Feb 28 2012 Rob Mokkink <rob@mokkinksystems.com> - 0.4-1
- Removed make load from install section
- Added bbswitch.conf to install section
- Added bbswitch.conf to the files section in kmodtool-bbswitch-el6.sh

* Sun Feb 26 2012 Rob Mokkink <rob@mokkinksystems.com> - 0.4-1
- Initial el6 build of the kmod package.
