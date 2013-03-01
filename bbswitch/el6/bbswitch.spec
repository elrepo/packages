# Define the kmod package name here.
%define kmod_name bbswitch

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 2.6.32-220.el6.%{_target_cpu}}

Name: %{kmod_name}-kmod
Version: 0.5
Release: 1%{?dist}
Group: System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL: https://github.com/Bumblebee-Project/bbswitch

BuildRequires: redhat-rpm-config
ExclusiveArch: i686 x86_64

# Sources.
Source0: %{kmod_name}-%{version}.tar.gz
Source5: GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el6.sh

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
bbswitch is a kernel module which automatically detects the required ACPI calls 
for two kinds of Optimus laptops. It has been verified to work with "real" Optimus 
and "legacy" Optimus laptops (at least, that is how I call them). The machines 
on which these tests has performed are:
    Clevo B7130 - GT 425M ("real" Optimus, Lekensteyns laptop)
    Dell Vostro 3500 - GT 310M ("legacy" Optimus, Samsagax' laptop)
(note: there is no need to add more supported laptops here as the universal calls 
should work for every laptop model supporting either Optimus calls)

It is preferred over manually hacking with the acpi_call module because it can detect 
the correct handle preceding _DSM and has some built-in safeguards:

You are not allowed to disable a card if a driver (nouveau, nvidia) is loaded.
Before suspend, the card is automatically enabled. When resuming, it is disabled again
if that was the case before suspending. Hibernation should work, but it not tested.

%prep
%setup -q -n %{kmod_name}-%{version}
%{__cp} -a %{SOURCE5} .
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf
echo "options %{kmod_name} load_state=0 unload_state=1" > %{kmod_name}.conf

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
%{__install} -d %{buildroot}%{_sysconfdir}/modprobe.d/
%{__install} %{kmod_name}.conf %{buildroot}/%{_sysconfdir}/modprobe.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} GPL-v2.0.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} NEWS %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} README.md %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/README
# Set the module(s) to be executable, so that they will be stripped when packaged.
find %{buildroot} -type f -name \*.ko -exec %{__chmod} u+x \{\} \;
# Remove the unrequired files.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

%clean
%{__rm} -rf %{buildroot}

%changelog
* Fri Mar  1 2013 Akemi Yagi <toracat@elrepo.org> - 0.5-1
- Upgraded to version 0.5 of bbswitch

* Sat Jun 23 2012 Rob Mokkink <rob@mokkinksystems.com> - 0.4.2-1
- Upgraded to version 0.4.2 of bbswitch

* Thu Feb 28 2012 Rob Mokkink <rob@mokkinksystems.com> - 0.4.1-2
- Removed make load from install section
- Added bbswitch.conf to install section

* Sun Feb 26 2012 Rob Mokkink <rob@mokkinksystems.com> - 0.4.1-1
- Initial el6 build of the kmod package.
