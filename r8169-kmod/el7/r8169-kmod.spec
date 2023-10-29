# Define the kmod package name here.
%define kmod_name r8169

# If kversion isn't defined on the rpmbuild line, define it here.
%{!?kversion: %define kversion 3.10.0-693.el7.%{_target_cpu}}

Name:    %{kmod_name}-kmod
Version: 6.020.00
Release: 2%{?dist}
Group:   System Environment/Kernel
License: GPLv2
Summary: %{kmod_name} kernel module(s)
URL:     http://www.realtek.com.tw/

BuildRequires: redhat-rpm-config, perl
ExclusiveArch: x86_64

# Sources.
Source0:  %{kmod_name}-%{version}.tar.gz
Source5:  GPL-v2.0.txt
Source10: kmodtool-%{kmod_name}-el7.sh
Source20: ELRepo-Makefile-%{kmod_name}

# Patches.
Patch0: ELRepo-r8169-dlink_pci_ids.patch

# Magic hidden here.
%{expand:%(sh %{SOURCE10} rpmtemplate %{kmod_name} %{kversion} "")}

# Disable the building of the debug package(s).
%define debug_package %{nil}

%description
This package provides the %{kmod_name} kernel modules for Realtek RTL8169, RTL8169SB(L),
RTL8169SC(L), RTL8110S-32, RTL8110SB(L) and RTL8110SC(L) Gigabit Ethernet controllers
with the PCI-Express interface.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -q -n %{kmod_name}-%{version}
%patch0 -p1
%{__rm} -f src/Makefile*
%{__cp} -a %{SOURCE20} src/Makefile
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

%build
KSRC=%{_usrsrc}/kernels/%{kversion}
%{__make} -C "${KSRC}" %{?_smp_mflags} modules M=$PWD/src

%install
%{__install} -d %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} src/%{kmod_name}.ko %{buildroot}/lib/modules/%{kversion}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} %{SOURCE5} %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} readme %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/

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
* Mon Oct 02 2017 Philip J Perry <phil@elrepo.org> - 6.020.00-2
- Fix build issues on RHEL 7.4

* Mon Jul 27 2015 Alan Bartlett <ajb@elrepo.org> - 6.020.00-1
- Updated to version 6.020.00

* Wed Nov 26 2014 Alan Bartlett <ajb@elrepo.org> - 6.019.00-2
- Applied patch to provide support for the DGE-530T Gigabit
- Ethernet Adapter (rev.C1) [http://elrepo.org/bugs/view.php?id=534]

* Wed Oct 08 2014 Alan Bartlett <ajb@elrepo.org> - 6.019.00-1
- Initial el7 build of the kmod package.
