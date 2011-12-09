# $Id$
# Authority: dag

# Define the kmod package name here.
%define kmod_name ndiswrapper

# If kversion isn't defined on the rpmbuild line, define it here.
# RHEL-6.2 kernel breaks kABI cpmpatibility.
%{!?kversion:%define kversion 2.6.32-220.el6.%{_target_cpu}}

Summary: %{kmod_name} kernel module(s)
Name: %{kmod_name}-kmod
Version: 1.56
Release: 1%{?dist}
License: GPL v2
Group: System Environment/Kernel
URL: http://ndiswrapper.sourceforge.net/

# Sources.
Source0: http://heanet.dl.sourceforge.net/project/ndiswrapper/stable/%{version}/ndiswrapper-%{version}.tar.gz
Source10: kmodtool-%{kmod_name}-el6.sh
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-build-%(%{__id_u} -n)

ExclusiveArch: i686 x86_64
BuildRequires: redhat-rpm-config
BuildRequires: rpm-build

# Magic hidden here.
%define kmodtool sh %{SOURCE10}
%{expand:%(%{kmodtool} rpmtemplate %{kmod_name} %{kversion} "" 2>/dev/null)}

# Disable the building of the debug package(s).
%define	debug_package %{nil}

%description
This package provides the %{kmod_name} kernel module(s).
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.

%prep
%setup -n %{kmod_name}-%{version}
%{__cat} <<EOF >kmod-%{kmod_name}.conf
override %{kmod_name} * weak-updates/%{kmod_name}
EOF

%build
ksrc="%{_usrsrc}/kernels/%{kversion}"
%{__make} -C "$ksrc" %{?_smp_mflags} modules M="$PWD/driver"
#%{__make} -C "$PWD" KVERS="%{kversion}" KBUILD="$ksrc"

%install
%{__rm} -rf %{buildroot}
export INSTALL_MOD_PATH="%{buildroot}"
export INSTALL_MOD_DIR="extra/%{kmod_name}"
ksrc="%{_usrsrc}/kernels/%{kversion}"
%{__make} -C "$ksrc" modules_install M="$PWD/driver"
%{__install} -Dp -m0644 kmod-%{kmod_name}.conf %{buildroot}/etc/depmod.d/kmod-%{kmod_name}.conf
# Strip the module(s).
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;
# Remove the files that are not required.
%{__rm} -f %{buildroot}/lib/modules/%{kversion}/modules.*

### Move documentation in place
for file in AUTHORS ChangeLog INSTALL README; do
    %{__install} -Dp -m0644 $file %{buildroot}%{_docdir}/kmod-%{kmod_name}-%{version}/$file
done

%clean
%{__rm} -rf %{buildroot}

%changelog
* Mon Nov 29 2010 Dag Wieers <dag@elrepo.org> - 1.56-1
- Initial package for RHEL6.
